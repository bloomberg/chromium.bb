// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "build/util/webkit_version.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_op/device_args.pb.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/public/proto/assistant_device_settings_ui.pb.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/assistant/service.h"
#include "chromeos/services/assistant/utils.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/internal_api/media_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/gurl.h"

using ActionModule = assistant_client::ActionModule;
using Resolution = assistant_client::ConversationStateListener::Resolution;

namespace api = ::assistant::api;

namespace chromeos {
namespace assistant {

const char kWiFiDeviceSettingId[] = "WIFI";
const char kBluetoothDeviceSettingId[] = "BLUETOOTH";

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    service_manager::Connector* connector,
    device::mojom::BatteryMonitorPtr battery_monitor,
    Service* service,
    bool enable_hotword)
    : platform_api_(connector, std::move(battery_monitor), enable_hotword),
      enable_hotword_(enable_hotword),
      action_module_(std::make_unique<action::CrosActionModule>(this)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      display_connection_(std::make_unique<CrosDisplayConnection>(this)),
      voice_interaction_observer_binding_(this),
      service_(service),
      background_thread_("background thread"),
      weak_factory_(this) {
  background_thread_.Start();
  connector->BindInterface(ash::mojom::kServiceName,
                           &voice_interaction_controller_);

  ash::mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_observer_binding_.Bind(mojo::MakeRequest(&ptr));
  voice_interaction_controller_->AddObserver(std::move(ptr));
}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {}

void AssistantManagerServiceImpl::Start(const std::string& access_token,
                                        base::OnceClosure post_init_callback) {
  // Set the flag to avoid starting the service multiple times.
  state_ = State::STARTED;

  // LibAssistant creation will make file IO and sync wait. Post the creation to
  // background thread to avoid DCHECK.
  background_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::StartAssistantInternal,
                     base::Unretained(this), access_token,
                     chromeos::version_loader::GetARCVersion()),
      base::BindOnce(&AssistantManagerServiceImpl::PostInitAssistant,
                     base::Unretained(this), std::move(post_init_callback)));
}

AssistantManagerService::State AssistantManagerServiceImpl::GetState() const {
  return state_;
}

void AssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {
  // Push the |access_token| we got as an argument into AssistantManager before
  // starting to ensure that all server requests will be authenticated once
  // it is started. |user_id| is used to pair a user to their |access_token|,
  // since we do not support multi-user in this example we can set it to a
  // dummy value like "0".
  assistant_manager_->SetAuthTokens({std::pair<std::string, std::string>(
      /* user_id: */ "0", access_token)});
}

void AssistantManagerServiceImpl::RegisterFallbackMediaHandler() {
  // Register handler for media actions.
  auto* media_manager = assistant_manager_internal_->GetMediaManager();
  media_manager->RegisterFallbackMediaHandler(
      [this](std::string play_media_args_proto) {
        std::string url = GetWebUrlFromMediaArgs(play_media_args_proto);
        if (!url.empty()) {
          OnOpenUrl(url);
        }
      });
}

void AssistantManagerServiceImpl::EnableListening(bool enable) {
  assistant_manager_->EnableListening(enable);
}

AssistantSettingsManager*
AssistantManagerServiceImpl::GetAssistantSettingsManager() {
  return assistant_settings_manager_.get();
}

void AssistantManagerServiceImpl::SendGetSettingsUiRequest(
    const std::string& selector,
    GetSettingsUiResponseCallback callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  std::string serialized_proto = SerializeGetSettingsUiRequest(selector);
  assistant_manager_internal_->SendGetSettingsUiRequest(
      serialized_proto, std::string(), [
        repeating_callback =
            base::AdaptCallbackForRepeating(std::move(callback)),
        weak_ptr = weak_factory_.GetWeakPtr(),
        task_runner = main_thread_task_runner_
      ](const assistant_client::VoicelessResponse& response) {
        // This callback may be called from server multiple times. We should
        // only process non-empty response.
        std::string settings = UnwrapGetSettingsUiResponse(response);
        if (!settings.empty()) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &AssistantManagerServiceImpl::HandleGetSettingsResponse,
                  std::move(weak_ptr), repeating_callback, settings));
        }
      });
}

void AssistantManagerServiceImpl::SendUpdateSettingsUiRequest(
    const std::string& update,
    UpdateSettingsUiResponseCallback callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  std::string serialized_proto = SerializeUpdateSettingsUiRequest(update);
  assistant_manager_internal_->SendUpdateSettingsUiRequest(
      serialized_proto, std::string(), [
        repeating_callback =
            base::AdaptCallbackForRepeating(std::move(callback)),
        weak_ptr = weak_factory_.GetWeakPtr(),
        task_runner = main_thread_task_runner_
      ](const assistant_client::VoicelessResponse& response) {
        // This callback may be called from server multiple times. We should
        // only process non-empty response.
        std::string update = UnwrapUpdateSettingsUiResponse(response);
        if (!update.empty()) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &AssistantManagerServiceImpl::HandleUpdateSettingsResponse,
                  std::move(weak_ptr), repeating_callback, update));
        }
      });
}

void AssistantManagerServiceImpl::RequestScreenContext(
    const gfx::Rect& region,
    RequestScreenContextCallback callback) {
  if (!assistant_enabled_ || !context_enabled_) {
    // If context is not enabled, we notify assistant immediately to activate
    // the UI.
    std::move(callback).Run();
    return;
  }

  // We wait for the closure to execute twice: once for the screenshot and once
  // for the view hierarchy.
  auto on_done = base::BarrierClosure(
      2, base::BindOnce(
             &AssistantManagerServiceImpl::SendContextQueryAndRunCallback,
             weak_factory_.GetWeakPtr(), std::move(callback)));

  service_->client()->RequestAssistantStructure(
      base::BindOnce(&AssistantManagerServiceImpl::OnAssistantStructureReceived,
                     weak_factory_.GetWeakPtr(), on_done));

  // TODO(muyuanli): handle metalayer and grab only part of the screen.
  service_->assistant_controller()->RequestScreenshot(
      region, base::BindOnce(
                  &AssistantManagerServiceImpl::OnAssistantScreenshotReceived,
                  weak_factory_.GetWeakPtr(), on_done));
}

void AssistantManagerServiceImpl::StartVoiceInteraction() {
  platform_api_.SetMicState(true);
  assistant_manager_->StartAssistantInteraction();
}

void AssistantManagerServiceImpl::StopActiveInteraction() {
  platform_api_.SetMicState(false);
  assistant_manager_->StopAssistantInteraction();
}

void AssistantManagerServiceImpl::SendTextQuery(const std::string& query) {
  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;
  options.modality =
      assistant_client::VoicelessOptions::Modality::TYPING_MODALITY;

  std::string interaction = CreateTextQueryInteraction(query);
  assistant_manager_internal_->SendVoicelessInteraction(
      interaction, /*description=*/"text_query", options, [](auto) {});
}

void AssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    mojom::AssistantInteractionSubscriberPtr subscriber) {
  interaction_subscribers_.AddPtr(std::move(subscriber));
}

void AssistantManagerServiceImpl::AddAssistantNotificationSubscriber(
    mojom::AssistantNotificationSubscriberPtr subscriber) {
  notification_subscribers_.AddPtr(std::move(subscriber));
}

void AssistantManagerServiceImpl::RetrieveNotification(
    mojom::AssistantNotificationPtr notification,
    int action_index) {
  const std::string& notification_id = notification->notification_id;
  const std::string& consistency_token = notification->consistency_token;
  const std::string& opaque_token = notification->opaque_token;

  const std::string request_interaction =
      SerializeNotificationRequestInteraction(
          notification_id, consistency_token, opaque_token, action_index);

  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;

  assistant_manager_internal_->SendVoicelessInteraction(
      request_interaction, "RequestNotification", options, [](auto) {});
}

void AssistantManagerServiceImpl::DismissNotification(
    mojom::AssistantNotificationPtr notification) {
  const std::string& notification_id = notification->notification_id;
  const std::string& consistency_token = notification->consistency_token;
  const std::string& opaque_token = notification->opaque_token;
  const std::string& grouping_key = notification->grouping_key;

  const std::string dismissed_interaction =
      SerializeNotificationDismissedInteraction(
          notification_id, consistency_token, opaque_token, {grouping_key});

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = notification->obfuscated_gaia_id;

  assistant_manager_internal_->SendVoicelessInteraction(
      dismissed_interaction, "DismissNotification", options, [](auto) {});
}

void AssistantManagerServiceImpl::OnConversationTurnStarted(bool is_mic_open) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnConversationTurnStartedOnMainThread,
          weak_factory_.GetWeakPtr(), is_mic_open));
}

void AssistantManagerServiceImpl::OnConversationTurnFinished(
    Resolution resolution) {
  // TODO(updowndota): Find a better way to handle the edge cases.
  if (resolution != Resolution::NORMAL_WITH_FOLLOW_ON &&
      resolution != Resolution::CANCELLED &&
      resolution != Resolution::BARGE_IN) {
    platform_api_.SetMicState(false);
  }
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnConversationTurnFinishedOnMainThread,
          weak_factory_.GetWeakPtr(), resolution));
}

void AssistantManagerServiceImpl::OnShowHtml(const std::string& html) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnShowHtmlOnMainThread,
                     weak_factory_.GetWeakPtr(), html));
}

void AssistantManagerServiceImpl::OnShowSuggestions(
    const std::vector<action::Suggestion>& suggestions) {
  // Convert to mojom struct for IPC.
  std::vector<mojom::AssistantSuggestionPtr> ptrs;
  for (const action::Suggestion& suggestion : suggestions) {
    mojom::AssistantSuggestionPtr ptr = mojom::AssistantSuggestion::New();
    ptr->text = suggestion.text;
    ptr->icon_url = GURL(suggestion.icon_url);
    ptr->action_url = GURL(suggestion.action_url);
    ptrs.push_back(std::move(ptr));
  }

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnShowSuggestionsOnMainThread,
          weak_factory_.GetWeakPtr(), std::move(ptrs)));
}

void AssistantManagerServiceImpl::OnShowText(const std::string& text) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnShowTextOnMainThread,
                     weak_factory_.GetWeakPtr(), text));
}

void AssistantManagerServiceImpl::OnOpenUrl(const std::string& url) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnOpenUrlOnMainThread,
                     weak_factory_.GetWeakPtr(), url));
}

void AssistantManagerServiceImpl::OnShowNotification(
    const action::Notification& notification) {
  mojom::AssistantNotificationPtr notification_ptr =
      mojom::AssistantNotification::New();
  notification_ptr->title = notification.title;
  notification_ptr->message = notification.text;
  notification_ptr->action_url = GURL(notification.action_url);
  notification_ptr->notification_id = notification.notification_id;
  notification_ptr->consistency_token = notification.consistency_token;
  notification_ptr->opaque_token = notification.opaque_token;
  notification_ptr->grouping_key = notification.grouping_key;
  notification_ptr->obfuscated_gaia_id = notification.obfuscated_gaia_id;

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnShowNotificationOnMainThread,
          weak_factory_.GetWeakPtr(), std::move(notification_ptr)));
}

void AssistantManagerServiceImpl::OnRecognitionStateChanged(
    assistant_client::ConversationStateListener::RecognitionState state,
    const assistant_client::ConversationStateListener::RecognitionResult&
        recognition_result) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnRecognitionStateChangedOnMainThread,
          weak_factory_.GetWeakPtr(), state, recognition_result));
}

void AssistantManagerServiceImpl::OnSpeechLevelUpdated(
    const float speech_level) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnSpeechLevelUpdatedOnMainThread,
          weak_factory_.GetWeakPtr(), speech_level));
}

void AssistantManagerServiceImpl::OnModifySettingsAction(
    const std::string& modify_setting_args_proto) {
  api::client_op::ModifySettingArgs modify_setting_args;
  modify_setting_args.ParseFromString(modify_setting_args_proto);
  DCHECK(IsSettingSupported(modify_setting_args.setting_id()));

  // TODO(rcui): Add support for bluetooth, etc.
  if (modify_setting_args.setting_id() == kWiFiDeviceSettingId) {
    switch (modify_setting_args.change()) {
      case api::client_op::ModifySettingArgs_Change_ON:
        service_->device_actions()->SetWifiEnabled(true);
        return;
      case api::client_op::ModifySettingArgs_Change_OFF:
        service_->device_actions()->SetWifiEnabled(false);
        return;

      case api::client_op::ModifySettingArgs_Change_TOGGLE:
      case api::client_op::ModifySettingArgs_Change_INCREASE:
      case api::client_op::ModifySettingArgs_Change_DECREASE:
      case api::client_op::ModifySettingArgs_Change_SET:
      case api::client_op::ModifySettingArgs_Change_UNSPECIFIED:
        break;
    }
    DLOG(ERROR) << "Unsupported change operation: "
                << modify_setting_args.change() << " for setting "
                << modify_setting_args.setting_id();
  }
}

ActionModule::Result AssistantManagerServiceImpl::HandleModifySettingClientOp(
    const std::string& modify_setting_args_proto) {
  DVLOG(2) << "HandleModifySettingClientOp=" << modify_setting_args_proto;
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnModifySettingsAction,
                     weak_factory_.GetWeakPtr(), modify_setting_args_proto));
  return ActionModule::Result::Ok();
}

bool AssistantManagerServiceImpl::IsSettingSupported(
    const std::string& setting_id) {
  DVLOG(2) << "IsSettingSupported=" << setting_id;
  return (setting_id == kWiFiDeviceSettingId ||
          setting_id == kBluetoothDeviceSettingId);
}

bool AssistantManagerServiceImpl::SupportsModifySettings() {
  return true;
}

void AssistantManagerServiceImpl::OnNotificationRemoved(
    const std::string& grouping_key) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnNotificationRemovedOnMainThread,
          weak_factory_.GetWeakPtr(), grouping_key));
}

void AssistantManagerServiceImpl::OnVoiceInteractionSettingsEnabled(
    bool enabled) {
  assistant_enabled_ = enabled;
}

void AssistantManagerServiceImpl::OnVoiceInteractionContextEnabled(
    bool enabled) {
  context_enabled_ = enabled;
}

void AssistantManagerServiceImpl::OnVoiceInteractionSetupCompleted(
    bool completed) {
  UpdateDeviceLocale(completed);
}

void AssistantManagerServiceImpl::StartAssistantInternal(
    const std::string& access_token,
    const std::string& arc_version) {
  DCHECK(background_thread_.task_runner()->BelongsToCurrentThread());

  assistant_manager_.reset(assistant_client::AssistantManager::Create(
      &platform_api_, CreateLibAssistantConfig(!enable_hotword_)));
  assistant_manager_internal_ =
      UnwrapAssistantManagerInternal(assistant_manager_.get());

  auto* internal_options =
      assistant_manager_internal_->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options, BuildUserAgent(arc_version));
  assistant_manager_internal_->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });

  assistant_manager_internal_->SetDisplayConnection(display_connection_.get());
  assistant_manager_internal_->RegisterActionModule(action_module_.get());
  assistant_manager_internal_->SetAssistantManagerDelegate(this);
  assistant_manager_->AddConversationStateListener(this);
  assistant_manager_->AddDeviceStateListener(this);

  SetAccessToken(access_token);

  assistant_manager_->Start();
}

void AssistantManagerServiceImpl::PostInitAssistant(
    base::OnceClosure post_init_callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  state_ = State::RUNNING;

  if (!assistant_settings_manager_) {
    assistant_settings_manager_ =
        std::make_unique<AssistantSettingsManagerImpl>(this);
  }
  std::move(post_init_callback).Run();
  UpdateDeviceSettings();
}

std::string AssistantManagerServiceImpl::BuildUserAgent(
    const std::string& arc_version) const {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (X11; CrOS %s %d.%d.%d; %s) "
                      "AppleWebKit/%d.%d (KHTML, like Gecko)",
                      base::SysInfo::OperatingSystemArchitecture().c_str(),
                      os_major_version, os_minor_version, os_bugfix_version,
                      base::SysInfo::GetLsbReleaseBoard().c_str(),
                      WEBKIT_VERSION_MAJOR, WEBKIT_VERSION_MINOR);

  if (!arc_version.empty()) {
    base::StringAppendF(&user_agent, " ARC/%s", arc_version.c_str());
  }
  return user_agent;
}

void AssistantManagerServiceImpl::UpdateDeviceSettings() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  const std::string device_id = assistant_manager_->GetDeviceId();
  if (device_id.empty())
    return;

  // Update device id and device type.
  assistant::SettingsUiUpdate update;
  assistant::AssistantDeviceSettingsUpdate* device_settings_update =
      update.mutable_assistant_device_settings_update()
          ->add_assistant_device_settings_update();
  device_settings_update->set_device_id(device_id);
  device_settings_update->set_assistant_device_type(
      assistant::AssistantDevice::CROS);

  // Device settings update result is not handled because it is not included in
  // the SettingsUiUpdateResult.
  SendUpdateSettingsUiRequest(update.SerializeAsString(), base::DoNothing());

  // Update device locale if voice interaction setup is completed.
  AssistantManagerServiceImpl::IsVoiceInteractionSetupCompleted(
      base::BindOnce(&AssistantManagerServiceImpl::UpdateDeviceLocale,
                     weak_factory_.GetWeakPtr()));
}

void AssistantManagerServiceImpl::UpdateDeviceLocale(bool is_setup_completed) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (!is_setup_completed)
    return;

  // Update device locale.
  assistant::SettingsUiUpdate update;
  assistant::AssistantDeviceSettingsUpdate* device_settings_update =
      update.mutable_assistant_device_settings_update()
          ->add_assistant_device_settings_update();
  device_settings_update->mutable_device_settings()->set_locale(
      base::i18n::GetConfiguredLocale());

  // Device settings update result is not handled because it is not included in
  // the SettingsUiUpdateResult.
  SendUpdateSettingsUiRequest(update.SerializeAsString(), base::DoNothing());
}

void AssistantManagerServiceImpl::HandleGetSettingsResponse(
    base::RepeatingCallback<void(const std::string&)> callback,
    const std::string& settings) {
  callback.Run(settings);
}

void AssistantManagerServiceImpl::HandleUpdateSettingsResponse(
    base::RepeatingCallback<void(const std::string&)> callback,
    const std::string& result) {
  callback.Run(result);
}

void AssistantManagerServiceImpl::OnStartFinished() {
  RegisterFallbackMediaHandler();
}

void AssistantManagerServiceImpl::OnConversationTurnStartedOnMainThread(
    bool is_mic_open) {
  interaction_subscribers_.ForAllPtrs([is_mic_open](auto* ptr) {
    ptr->OnInteractionStarted(/*is_voice_interaction=*/is_mic_open);
  });
}

void AssistantManagerServiceImpl::OnConversationTurnFinishedOnMainThread(
    Resolution resolution) {
  switch (resolution) {
    // Interaction ended normally.
    // Note that TIMEOUT here does not refer to server timeout, but rather mic
    // timeout due to speech inactivity. As this case does not require special
    // UI logic, it is treated here as a normal interaction completion.
    case Resolution::NORMAL:
    case Resolution::NORMAL_WITH_FOLLOW_ON:
    case Resolution::TIMEOUT:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kNormal);
      });
      break;
    // Interaction ended due to interruption.
    case Resolution::BARGE_IN:
    case Resolution::CANCELLED:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kInterruption);
      });
      break;
    // Interaction ended due to multi-device hotword loss.
    case Resolution::NO_RESPONSE:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kMultiDeviceHotwordLoss);
      });
      break;
    // Interaction ended due to error.
    case Resolution::COMMUNICATION_ERROR:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kError);
      });
      break;
  }
}

void AssistantManagerServiceImpl::OnShowHtmlOnMainThread(
    const std::string& html) {
  interaction_subscribers_.ForAllPtrs(
      [&html](auto* ptr) { ptr->OnHtmlResponse(html); });
}

void AssistantManagerServiceImpl::OnShowSuggestionsOnMainThread(
    const std::vector<mojom::AssistantSuggestionPtr>& suggestions) {
  interaction_subscribers_.ForAllPtrs([&suggestions](auto* ptr) {
    ptr->OnSuggestionsResponse(mojo::Clone(suggestions));
  });
}

void AssistantManagerServiceImpl::OnShowTextOnMainThread(
    const std::string& text) {
  interaction_subscribers_.ForAllPtrs(
      [&text](auto* ptr) { ptr->OnTextResponse(text); });
}

void AssistantManagerServiceImpl::OnOpenUrlOnMainThread(
    const std::string& url) {
  interaction_subscribers_.ForAllPtrs(
      [&url](auto* ptr) { ptr->OnOpenUrlResponse(GURL(url)); });
}

void AssistantManagerServiceImpl::OnShowNotificationOnMainThread(
    const mojom::AssistantNotificationPtr& notification) {
  notification_subscribers_.ForAllPtrs([&notification](auto* ptr) {
    ptr->OnShowNotification(notification.Clone());
  });
}

void AssistantManagerServiceImpl::OnNotificationRemovedOnMainThread(
    const std::string& grouping_key) {
  notification_subscribers_.ForAllPtrs(
      [grouping_key](auto* ptr) { ptr->OnRemoveNotification(grouping_key); });
}

void AssistantManagerServiceImpl::OnRecognitionStateChangedOnMainThread(
    assistant_client::ConversationStateListener::RecognitionState state,
    const assistant_client::ConversationStateListener::RecognitionResult&
        recognition_result) {
  switch (state) {
    case assistant_client::ConversationStateListener::RecognitionState::STARTED:
      interaction_subscribers_.ForAllPtrs(
          [](auto* ptr) { ptr->OnSpeechRecognitionStarted(); });
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        INTERMEDIATE_RESULT:
      interaction_subscribers_.ForAllPtrs([&recognition_result](auto* ptr) {
        ptr->OnSpeechRecognitionIntermediateResult(
            recognition_result.high_confidence_text,
            recognition_result.low_confidence_text);
      });
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        END_OF_UTTERANCE:
      interaction_subscribers_.ForAllPtrs(
          [](auto* ptr) { ptr->OnSpeechRecognitionEndOfUtterance(); });
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        FINAL_RESULT:
      interaction_subscribers_.ForAllPtrs([&recognition_result](auto* ptr) {
        ptr->OnSpeechRecognitionFinalResult(
            recognition_result.recognized_speech);
      });
      break;
  }
}

void AssistantManagerServiceImpl::OnSpeechLevelUpdatedOnMainThread(
    const float speech_level) {
  interaction_subscribers_.ForAllPtrs(
      [&speech_level](auto* ptr) { ptr->OnSpeechLevelUpdated(speech_level); });
}

void AssistantManagerServiceImpl::IsVoiceInteractionSetupCompleted(
    ash::mojom::VoiceInteractionController::IsSetupCompletedCallback callback) {
  voice_interaction_controller_->IsSetupCompleted(std::move(callback));
}

void AssistantManagerServiceImpl::OnAssistantStructureReceived(
    base::OnceClosure on_done,
    ax::mojom::AssistantExtraPtr assistant_extra,
    std::unique_ptr<ui::AssistantTree> assistant_tree) {
  assistant_extra_ = std::move(assistant_extra);
  assistant_tree_ = std::move(assistant_tree);
  std::move(on_done).Run();
}

void AssistantManagerServiceImpl::OnAssistantScreenshotReceived(
    base::OnceClosure on_done,
    const std::vector<uint8_t>& jpg_image) {
  assistant_screenshot_ = jpg_image;
  std::move(on_done).Run();
}

void AssistantManagerServiceImpl::SendContextQueryAndRunCallback(
    RequestScreenContextCallback callback) {
  assistant_manager_internal_->SendScreenContextRequest(
      std::vector<std::string>{
          CreateContextProto(AssistantBundle{
              std::move(assistant_extra_), std::move(assistant_tree_),
          }),
          CreateContextProto(assistant_screenshot_)});
  assistant_screenshot_.clear();
  std::move(callback).Run();
}

}  // namespace assistant
}  // namespace chromeos
