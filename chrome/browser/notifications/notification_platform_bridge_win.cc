// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_win.h"

#include <NotificationActivationCallback.h>
#include <activation.h>
#include <wrl.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/win/core_winrt_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_image_retainer.h"
#include "chrome/browser/notifications/notification_launch_id.h"
#include "chrome/browser/notifications/notification_platform_bridge_win_metrics.h"
#include "chrome/browser/notifications/notification_template_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_thread.h"
#include "ui/message_center/public/cpp/notification.h"

namespace mswr = Microsoft::WRL;
namespace mswrw = Microsoft::WRL::Wrappers;

namespace winfoundtn = ABI::Windows::Foundation;
namespace winui = ABI::Windows::UI;
namespace winxml = ABI::Windows::Data::Xml;

using base::win::ScopedHString;
using message_center::RichNotificationData;
using notifications_uma::ActivationStatus;
using notifications_uma::CloseStatus;
using notifications_uma::DisplayStatus;
using notifications_uma::GetDisplayedLaunchIdStatus;
using notifications_uma::GetDisplayedStatus;
using notifications_uma::GetNotificationLaunchIdStatus;
using notifications_uma::HandleEventStatus;
using notifications_uma::HistoryStatus;
using notifications_uma::SetReadyCallbackStatus;

namespace {

// This string needs to be max 16 characters to work on Windows 10 prior to
// applying Creators Update (build 15063).
const wchar_t kGroup[] = L"Notifications";

typedef winfoundtn::ITypedEventHandler<
    winui::Notifications::ToastNotification*,
    winui::Notifications::ToastDismissedEventArgs*>
    ToastDismissedHandler;
typedef winfoundtn::ITypedEventHandler<
    winui::Notifications::ToastNotification*,
    winui::Notifications::ToastFailedEventArgs*>
    ToastFailedHandler;

// Templated wrapper for winfoundtn::GetActivationFactory().
template <unsigned int size, typename T>
HRESULT CreateActivationFactory(wchar_t const (&class_name)[size], T** object) {
  ScopedHString ref_class_name =
      ScopedHString::Create(base::StringPiece16(class_name, size - 1));
  return base::win::RoGetActivationFactory(ref_class_name.get(),
                                           IID_PPV_ARGS(object));
}

void ForwardNotificationOperationOnUiThread(
    NotificationCommon::Operation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const std::string& profile_id,
    bool incognito,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const base::Optional<bool>& by_user) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_browser_process)
    return;

  g_browser_process->profile_manager()->LoadProfile(
      profile_id, incognito,
      base::Bind(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                 operation, notification_type, origin, notification_id,
                 action_index, reply, by_user));
}

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeWin();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

class NotificationPlatformBridgeWinImpl
    : public base::RefCountedThreadSafe<NotificationPlatformBridgeWinImpl>,
      public content::BrowserThread::DeleteOnThread<
          content::BrowserThread::UI> {
 public:
  explicit NotificationPlatformBridgeWinImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : image_retainer_(
            std::make_unique<NotificationImageRetainer>(task_runner)),
        task_runner_(std::move(task_runner)) {
    DCHECK(task_runner_);
    com_functions_initialized_ =
        base::win::ResolveCoreWinRTDelayload() &&
        ScopedHString::ResolveCoreWinRTStringDelayload();
  }

  // Obtain an IToastNotification interface from a given XML (provided by the
  // NotificationTemplateBuilder). This function is only used when displaying
  // notification in production code, which explains why the UMA metrics record
  // within are classified with the display path.
  HRESULT GetToastNotification(
      const message_center::Notification& notification,
      const NotificationTemplateBuilder& notification_template_builder,
      const std::string& profile_id,
      bool incognito,
      winui::Notifications::IToastNotification** toast_notification) {
    *toast_notification = nullptr;

    ScopedHString ref_class_name =
        ScopedHString::Create(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument);
    mswr::ComPtr<IInspectable> inspectable;
    HRESULT hr =
        base::win::RoActivateInstance(ref_class_name.get(), &inspectable);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::RO_ACTIVATE_FAILED);
      DLOG(ERROR) << "Unable to activate the XML Document";
      return hr;
    }

    mswr::ComPtr<winxml::Dom::IXmlDocumentIO> document_io;
    hr = inspectable.As<winxml::Dom::IXmlDocumentIO>(&document_io);
    if (FAILED(hr)) {
      LogDisplayHistogram(
          DisplayStatus::CONVERSION_FAILED_INSPECTABLE_TO_XML_IO);
      DLOG(ERROR) << "Failed to get XmlDocument as IXmlDocumentIO";
      return hr;
    }

    base::string16 notification_template =
        notification_template_builder.GetNotificationTemplate();
    ScopedHString ref_template = ScopedHString::Create(notification_template);
    hr = document_io->LoadXml(ref_template.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::LOAD_XML_FAILED);
      DLOG(ERROR) << "Unable to load the template's XML into the document";
      return hr;
    }

    mswr::ComPtr<winxml::Dom::IXmlDocument> document;
    hr = document_io.As<winxml::Dom::IXmlDocument>(&document);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CONVERSION_FAILED_XML_IO_TO_XML);
      DLOG(ERROR) << "Unable to get as XMLDocument";
      return hr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationFactory>
        toast_notification_factory;
    hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotification,
        toast_notification_factory.GetAddressOf());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_FACTORY_FAILED);
      DLOG(ERROR) << "Unable to create the IToastNotificationFactory";
      return hr;
    }

    hr = toast_notification_factory->CreateToastNotification(
        document.Get(), toast_notification);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_TOAST_NOTIFICATION_FAILED);
      DLOG(ERROR) << "Unable to create the IToastNotification";
      return hr;
    }

    winui::Notifications::IToastNotification2* toast2ptr = nullptr;
    hr = (*toast_notification)->QueryInterface(&toast2ptr);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_TOAST_NOTIFICATION2_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotification2 object";
      return hr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
    toast2.Attach(toast2ptr);

    // Set the Group and Tag values for the notification, in order to support
    // closing/replacing notification by tag. Both of these values have a limit
    // of 64 characters, which is problematic because they are out of our
    // control and Tag can contain just about anything. Therefore we use a hash
    // of the Tag value to produce uniqueness that fits within the specified
    // limits. Although Group is hard-coded, uniqueness is guaranteed through
    // features providing a sufficiently distinct notification.id().
    ScopedHString group = ScopedHString::Create(kGroup);
    ScopedHString tag = ScopedHString::Create(GetTag(notification.id()));

    hr = toast2->put_Group(group.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::SETTING_GROUP_FAILED);
      DLOG(ERROR) << "Failed to set Group";
      return hr;
    }

    hr = toast2->put_Tag(tag.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::SETTING_TAG_FAILED);
      DLOG(ERROR) << "Failed to set Tag";
      return hr;
    }

    // By default, Windows 10 will always show the notification on screen.
    // Chrome, however, wants to suppress them if both conditions are true:
    // 1) Renotify flag is not set.
    // 2) They are not new (no other notification with same tag is found).
    if (!notification.renotify()) {
      std::vector<winui::Notifications::IToastNotification*> notifications;
      GetNotifications(profile_id, incognito, &notifications);

      for (winui::Notifications::IToastNotification* notification :
           notifications) {
        winui::Notifications::IToastNotification2* t2 = nullptr;
        HRESULT hr = notification->QueryInterface(&t2);
        if (FAILED(hr))
          continue;

        HSTRING hstring_group;
        hr = t2->get_Group(&hstring_group);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::GET_GROUP_FAILED);
          DLOG(ERROR) << "Failed to get group value";
          return hr;
        }
        ScopedHString scoped_group(hstring_group);

        HSTRING hstring_tag;
        hr = t2->get_Tag(&hstring_tag);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::GET_TAG_FAILED);
          DLOG(ERROR) << "Failed to get tag value";
          return hr;
        }
        ScopedHString scoped_tag(hstring_tag);

        if (group.Get() != scoped_group.Get() || tag.Get() != scoped_tag.Get())
          continue;  // Because it is not a repeat of an toast.

        hr = toast2->put_SuppressPopup(true);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::SUPPRESS_POPUP_FAILED);
          DLOG(ERROR) << "Failed to set suppress value";
          return hr;
        }
      }
    }

    return S_OK;
  }

  void Display(NotificationHandler::Type notification_type,
               const std::string& profile_id,
               bool incognito,
               std::unique_ptr<message_center::Notification> notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) {
    // TODO(finnur): Move this to a RoInitialized thread, as per
    // crbug.com/761039.
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (!notifier_.Get() && FAILED(InitializeToastNotifier())) {
      // A histogram should have already been logged for this failure.
      DLOG(ERROR) << "Unable to initialize toast notifier";
      return;
    }

    winui::Notifications::NotificationSetting setting;
    HRESULT hr = notifier_->get_Setting(&setting);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::GET_NOTIFICATION_SETTING_FAILED);
      DLOG(ERROR) << "Unable to fetch notification settings";
      return;
    }

    switch (setting) {
      case winui::Notifications::NotificationSetting_Enabled:
        break;
      case winui::Notifications::NotificationSetting_DisabledForApplication:
        LogDisplayHistogram(DisplayStatus::DISABLED_FOR_APPLICATION);
        DLOG(ERROR) << "Notification disabled for application";
        return;
      case winui::Notifications::NotificationSetting_DisabledForUser:
        LogDisplayHistogram(DisplayStatus::DISABLED_FOR_USER);
        DLOG(ERROR) << "Notification disabled for user";
        return;
      case winui::Notifications::NotificationSetting_DisabledByGroupPolicy:
        LogDisplayHistogram(DisplayStatus::DISABLED_BY_GROUP_POLICY);
        DLOG(ERROR) << "Notification disabled by group policy";
        return;
      case winui::Notifications::NotificationSetting_DisabledByManifest:
        LogDisplayHistogram(DisplayStatus::DISABLED_BY_MANIFEST);
        DLOG(ERROR) << "Notification disabled by manifest";
        return;
    }

    NotificationLaunchId launch_id(notification_type, notification->id(),
                                   profile_id, incognito,
                                   notification->origin_url());
    std::unique_ptr<NotificationTemplateBuilder> notification_template =
        NotificationTemplateBuilder::Build(image_retainer_.get(), launch_id,
                                           profile_id, *notification);
    mswr::ComPtr<winui::Notifications::IToastNotification> toast;
    hr = GetToastNotification(*notification, *notification_template, profile_id,
                              incognito, &toast);
    if (FAILED(hr)) {
      // A histogram should have already been logged for this failure.
      DLOG(ERROR) << "Unable to get a toast notification";
      return;
    }

    // Activation via user interaction with the toast is handled in
    // HandleActivation() by way of the notification_helper.

    auto dismissed_handler = mswr::Callback<ToastDismissedHandler>(
        this, &NotificationPlatformBridgeWinImpl::OnDismissed);
    EventRegistrationToken dismissed_token;
    hr = toast->add_Dismissed(dismissed_handler.Get(), &dismissed_token);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::ADD_TOAST_DISMISS_HANDLER_FAILED);
      DLOG(ERROR) << "Unable to add toast dismissed event handler";
      return;
    }

    auto failed_handler = mswr::Callback<ToastFailedHandler>(
        this, &NotificationPlatformBridgeWinImpl::OnFailed);
    EventRegistrationToken failed_token;
    hr = toast->add_Failed(failed_handler.Get(), &failed_token);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::ADD_TOAST_ERROR_HANDLER_FAILED);
      DLOG(ERROR) << "Unable to add toast failed event handler";
      return;
    }

    hr = notifier_->Show(toast.Get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::SHOWING_TOAST_FAILED);
      DLOG(ERROR) << "Unable to display the notification";
    } else {
      LogDisplayHistogram(DisplayStatus::SUCCESS);
    }
  }

  void Close(const std::string& profile_id,
             const std::string& notification_id) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory> history;
    if (!GetIToastNotificationHistory(&history)) {
      LogCloseHistogram(CloseStatus::GET_TOAST_HISTORY_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory";
      return;
    }

    ScopedHString application_id = ScopedHString::Create(GetAppId());
    ScopedHString group = ScopedHString::Create(kGroup);
    ScopedHString tag = ScopedHString::Create(GetTag(notification_id));

    HRESULT hr = history->RemoveGroupedTagWithId(tag.get(), group.get(),
                                                 application_id.get());
    if (FAILED(hr)) {
      LogCloseHistogram(CloseStatus::REMOVING_TOAST_FAILED);
      DLOG(ERROR) << "Failed to remove notification with id "
                  << notification_id.c_str();
    } else {
      LogCloseHistogram(CloseStatus::SUCCESS);
    }
  }

  bool GetIToastNotificationHistory(
      winui::Notifications::IToastNotificationHistory** notification_history)
      const WARN_UNUSED_RESULT {
    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics>
        toast_manager;
    HRESULT hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
        toast_manager.GetAddressOf());
    if (FAILED(hr)) {
      LogHistoryHistogram(
          HistoryStatus::CREATE_TOAST_NOTIFICATION_MANAGER_FAILED);
      DLOG(ERROR) << "Unable to create the ToastNotificationManager";
      return false;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics2>
        toast_manager2;
    hr = toast_manager
             .As<winui::Notifications::IToastNotificationManagerStatics2>(
                 &toast_manager2);
    if (FAILED(hr)) {
      LogHistoryHistogram(
          HistoryStatus::QUERY_TOAST_MANAGER_STATISTICS2_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationManagerStatics2";
      return false;
    }

    hr = toast_manager2->get_History(notification_history);
    if (FAILED(hr)) {
      LogHistoryHistogram(HistoryStatus::GET_TOAST_HISTORY_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory";
      return false;
    }

    LogHistoryHistogram(HistoryStatus::SUCCESS);
    return true;
  }

  void GetDisplayedFromActionCenter(
      const std::string& profile_id,
      bool incognito,
      std::vector<winui::Notifications::IToastNotification*>* notifications)
      const {
    mswr::ComPtr<winui::Notifications::IToastNotificationHistory> history;
    if (!GetIToastNotificationHistory(&history)) {
      LogGetDisplayedStatus(GetDisplayedStatus::GET_TOAST_HISTORY_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory";
      return;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory2> history2;
    HRESULT hr =
        history.As<winui::Notifications::IToastNotificationHistory2>(&history2);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(
          GetDisplayedStatus::QUERY_TOAST_NOTIFICATION_HISTORY2_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory2";
      return;
    }

    ScopedHString application_id = ScopedHString::Create(GetAppId());

    mswr::ComPtr<winfoundtn::Collections::IVectorView<
        winui::Notifications::ToastNotification*>>
        list;
    hr = history2->GetHistoryWithId(application_id.get(), &list);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(GetDisplayedStatus::GET_HISTORY_WITH_ID_FAILED);
      DLOG(ERROR) << "GetHistoryWithId failed";
      return;
    }

    uint32_t size;
    hr = list->get_Size(&size);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(GetDisplayedStatus::GET_SIZE_FAILED);
      DLOG(ERROR) << "History get_Size call failed";
      return;
    }

    GetDisplayedStatus status = GetDisplayedStatus::SUCCESS;
    winui::Notifications::IToastNotification* tn;
    for (uint32_t index = 0; index < size; ++index) {
      hr = list->GetAt(0U, &tn);
      if (FAILED(hr)) {
        status = GetDisplayedStatus::SUCCESS_WITH_GET_AT_FAILURE;
        DLOG(ERROR) << "Failed to get notification " << index << " of " << size;
        continue;
      }
      notifications->push_back(tn);
    }

    LogGetDisplayedStatus(status);
  }

  void GetNotifications(const std::string& profile_id,
                        bool incognito,
                        std::vector<winui::Notifications::IToastNotification*>*
                            notifications) const {
    if (!NotificationPlatformBridgeWinImpl::notifications_for_testing_) {
      GetDisplayedFromActionCenter(profile_id, incognito, notifications);
    } else {
      *notifications =
          *NotificationPlatformBridgeWinImpl::notifications_for_testing_;
    }
  }

  void GetDisplayed(const std::string& profile_id,
                    bool incognito,
                    GetDisplayedNotificationsCallback callback) const {
    // TODO(finnur): Once this function is properly implemented, add DCHECK(UI)
    // to NotificationPlatformBridgeWin::GetDisplayed.
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    std::vector<winui::Notifications::IToastNotification*> notifications;
    GetNotifications(profile_id, incognito, &notifications);

    auto displayed_notifications = std::make_unique<std::set<std::string>>();
    for (winui::Notifications::IToastNotification* notification :
         notifications) {
      NotificationLaunchId launch_id(GetNotificationLaunchId(notification));
      if (!launch_id.is_valid()) {
        LogGetDisplayedLaunchIdStatus(
            GetDisplayedLaunchIdStatus::DECODE_LAUNCH_ID_FAILED);
        DLOG(ERROR) << "Failed to decode notification ID";
        continue;
      }
      if (launch_id.profile_id() != profile_id ||
          launch_id.incognito() != incognito)
        continue;
      LogGetDisplayedLaunchIdStatus(GetDisplayedLaunchIdStatus::SUCCESS);
      displayed_notifications->insert(launch_id.notification_id());
    }

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(callback), std::move(displayed_notifications),
                       /*supports_synchronization=*/true));
  }

  // Test to see if the notification_helper.exe has been registered in the
  // system, either under HKCU or HKLM.
  bool IsToastActivatorRegistered() {
    base::win::RegKey key;
    base::string16 path =
        InstallUtil::GetToastActivatorRegistryPath() + L"\\LocalServer32";
    HKEY root = install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                  : HKEY_CURRENT_USER;
    return ERROR_SUCCESS == key.Open(root, path.c_str(), KEY_QUERY_VALUE);
  }

  void SetReadyCallback(
      NotificationPlatformBridge::NotificationBridgeReadyCallback callback) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    bool activator_registered = IsToastActivatorRegistered();
    bool shortcut_installed =
        InstallUtil::IsStartMenuShortcutWithActivatorGuidInstalled();

    if (!shortcut_installed) {
      LogSetReadyCallbackStatus(
          SetReadyCallbackStatus::SHORTCUT_MISCONFIGURATION);
    } else if (!activator_registered) {
      LogSetReadyCallbackStatus(
          SetReadyCallbackStatus::COM_SERVER_MISCONFIGURATION);
    } else if (!com_functions_initialized_) {
      LogSetReadyCallbackStatus(SetReadyCallbackStatus::COM_NOT_INITIALIZED);
    } else {
      LogSetReadyCallbackStatus(SetReadyCallbackStatus::SUCCESS);
    }

    bool enabled = com_functions_initialized_ && activator_registered &&
                   shortcut_installed;

    bool success = content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(std::move(callback), enabled));
    DCHECK(success);
  }

  void HandleEvent(winui::Notifications::IToastNotification* notification,
                   NotificationCommon::Operation operation,
                   const base::Optional<int>& action_index,
                   const base::Optional<bool>& by_user) {
    NotificationLaunchId launch_id(GetNotificationLaunchId(notification));
    if (!launch_id.is_valid()) {
      LogHandleEventStatus(HandleEventStatus::HANDLE_EVENT_LAUNCH_ID_INVALID);
      DLOG(ERROR) << "Failed to decode launch ID for operation " << operation;
      return;
    }

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&ForwardNotificationOperationOnUiThread, operation,
                       launch_id.notification_type(), launch_id.origin_url(),
                       launch_id.notification_id(), launch_id.profile_id(),
                       launch_id.incognito(), action_index,
                       /*reply=*/base::nullopt, by_user));
    LogHandleEventStatus(HandleEventStatus::SUCCESS);
  }

  base::Optional<int> ParseActionIndex(
      winui::Notifications::IToastActivatedEventArgs* args) {
    HSTRING arguments;
    HRESULT hr = args->get_Arguments(&arguments);
    if (FAILED(hr))
      return base::nullopt;

    ScopedHString arguments_scoped(arguments);
    NotificationLaunchId launch_id(arguments_scoped.GetAsUTF8());
    if (!launch_id.is_valid() || launch_id.button_index() < 0)
      return base::nullopt;
    return launch_id.button_index();
  }

  void ForwardHandleEventForTesting(
      NotificationCommon::Operation operation,
      winui::Notifications::IToastNotification* notification,
      winui::Notifications::IToastActivatedEventArgs* args,
      const base::Optional<bool>& by_user) {
    base::Optional<int> action_index = ParseActionIndex(args);
    HandleEvent(notification, operation, action_index, by_user);
  }

 private:
  friend class base::RefCountedThreadSafe<NotificationPlatformBridgeWinImpl>;
  friend class NotificationPlatformBridgeWin;

  ~NotificationPlatformBridgeWinImpl() = default;

  base::string16 GetAppId() const {
    return ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall());
  }

  base::string16 GetTag(const std::string& notification_id) {
    return base::UintToString16(base::Hash(notification_id));
  }

  NotificationLaunchId GetNotificationLaunchId(
      winui::Notifications::IToastNotification* notification) const {
    mswr::ComPtr<winxml::Dom::IXmlDocument> document;
    HRESULT hr = notification->get_Content(&document);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::NOTIFICATION_GET_CONTENT_FAILED);
      DLOG(ERROR) << "Failed to get XML document";
      return NotificationLaunchId();
    }

    ScopedHString tag = ScopedHString::Create(kNotificationToastElement);
    mswr::ComPtr<winxml::Dom::IXmlNodeList> elements;
    hr = document->GetElementsByTagName(tag.get(), &elements);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::GET_ELEMENTS_BY_TAG_FAILED);
      DLOG(ERROR) << "Failed to get <toast> elements from document";
      return NotificationLaunchId();
    }

    UINT32 length;
    hr = elements->get_Length(&length);
    if (length == 0) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::MISSING_TOAST_ELEMENT_IN_DOC);
      DLOG(ERROR) << "No <toast> elements in document.";
      return NotificationLaunchId();
    }

    mswr::ComPtr<winxml::Dom::IXmlNode> node;
    hr = elements->Item(0, &node);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::ITEM_AT_FAILED);
      DLOG(ERROR) << "Failed to get first <toast> element";
      return NotificationLaunchId();
    }

    mswr::ComPtr<winxml::Dom::IXmlNamedNodeMap> attributes;
    hr = node->get_Attributes(&attributes);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::GET_ATTRIBUTES_FAILED);
      DLOG(ERROR) << "Failed to get attributes of <toast>";
      return NotificationLaunchId();
    }

    mswr::ComPtr<winxml::Dom::IXmlNode> leaf;
    ScopedHString id = ScopedHString::Create(kNotificationLaunchAttribute);
    hr = attributes->GetNamedItem(id.get(), &leaf);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::GET_NAMED_ITEM_FAILED);
      DLOG(ERROR) << "Failed to get launch attribute of <toast>";
      return NotificationLaunchId();
    }

    mswr::ComPtr<winxml::Dom::IXmlNode> child;
    hr = leaf->get_FirstChild(&child);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::GET_FIRST_CHILD_FAILED);
      DLOG(ERROR) << "Failed to get content of launch attribute";
      return NotificationLaunchId();
    }

    mswr::ComPtr<IInspectable> inspectable;
    hr = child->get_NodeValue(&inspectable);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::GET_NODE_VALUE_FAILED);
      DLOG(ERROR) << "Failed to get node value of launch attribute";
      return NotificationLaunchId();
    }

    mswr::ComPtr<winfoundtn::IPropertyValue> property_value;
    hr = inspectable.As<winfoundtn::IPropertyValue>(&property_value);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::CONVERSION_TO_PROP_VALUE_FAILED);
      DLOG(ERROR) << "Failed to convert node value of launch attribute";
      return NotificationLaunchId();
    }

    HSTRING value_hstring;
    hr = property_value->GetString(&value_hstring);
    if (FAILED(hr)) {
      LogGetNotificationLaunchIdStatus(
          GetNotificationLaunchIdStatus::GET_STRING_FAILED);
      DLOG(ERROR) << "Failed to get string for launch attribute";
      return NotificationLaunchId();
    }

    LogGetNotificationLaunchIdStatus(GetNotificationLaunchIdStatus::SUCCESS);

    ScopedHString value(value_hstring);
    return NotificationLaunchId(value.GetAsUTF8());
  }

  HRESULT OnDismissed(
      winui::Notifications::IToastNotification* notification,
      winui::Notifications::IToastDismissedEventArgs* arguments) {
    winui::Notifications::ToastDismissalReason reason;
    HRESULT hr = arguments->get_Reason(&reason);
    DCHECK(SUCCEEDED(hr));
    HandleEvent(
        notification, NotificationCommon::CLOSE,
        /*action_index=*/base::nullopt,
        reason == winui::Notifications::ToastDismissalReason_UserCanceled);
    return S_OK;
  }

  HRESULT OnFailed(winui::Notifications::IToastNotification* notification,
                   winui::Notifications::IToastFailedEventArgs* arguments) {
    HRESULT error_code;
    HRESULT hr = arguments->get_ErrorCode(&error_code);
    DCHECK(SUCCEEDED(hr));
    LOG(ERROR) << "Failed to raise the toast notification, error code: "
               << std::hex << error_code;
    return S_OK;
  }

  HRESULT InitializeToastNotifier() {
    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics>
        toast_manager;
    HRESULT hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
        toast_manager.GetAddressOf());
    if (FAILED(hr)) {
      LogDisplayHistogram(
          DisplayStatus::CREATE_TOAST_NOTIFICATION_MANAGER_FAILED);
      DLOG(ERROR) << "Unable to create the ToastNotificationManager";
      return hr;
    }

    ScopedHString application_id = ScopedHString::Create(GetAppId());
    hr = toast_manager->CreateToastNotifierWithId(application_id.get(),
                                                  &notifier_);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_TOAST_NOTIFIER_WITH_ID_FAILED);
      DLOG(ERROR) << "Unable to create the ToastNotifier";
    }
    return hr;
  }

  static std::vector<ABI::Windows::UI::Notifications::IToastNotification*>*
      notifications_for_testing_;

  // Whether the required functions from combase.dll have been loaded.
  bool com_functions_initialized_;

  // An object that keeps temp files alive long enough for Windows to pick up.
  std::unique_ptr<NotificationImageRetainer> image_retainer_;

  // The task runner this object runs on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The ToastNotifier to use to communicate with the Action Center.
  mswr::ComPtr<winui::Notifications::IToastNotifier> notifier_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWinImpl);
};

std::vector<ABI::Windows::UI::Notifications::IToastNotification*>*
    NotificationPlatformBridgeWinImpl::notifications_for_testing_ = nullptr;

NotificationPlatformBridgeWin::NotificationPlatformBridgeWin() {
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  impl_ = base::MakeRefCounted<NotificationPlatformBridgeWinImpl>(task_runner_);
}

NotificationPlatformBridgeWin::~NotificationPlatformBridgeWin() = default;

void NotificationPlatformBridgeWin::Display(
    NotificationHandler::Type notification_type,
    const std::string& profile_id,
    bool is_incognito,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make a deep copy of the notification as its resources cannot safely
  // be passed between threads.
  auto notification_copy = message_center::Notification::DeepCopy(
      notification, /*include_body_image=*/true, /*include_small_image=*/true,
      /*include_icon_images=*/true);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationPlatformBridgeWinImpl::Display, impl_,
                     notification_type, profile_id, is_incognito,
                     std::move(notification_copy), std::move(metadata)));
}

void NotificationPlatformBridgeWin::Close(const std::string& profile_id,
                                          const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NotificationPlatformBridgeWinImpl::Close,
                                impl_, notification_id, profile_id));
}

void NotificationPlatformBridgeWin::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    GetDisplayedNotificationsCallback callback) const {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationPlatformBridgeWinImpl::GetDisplayed, impl_,
                     profile_id, incognito, std::move(callback)));
}

void NotificationPlatformBridgeWin::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationPlatformBridgeWinImpl::SetReadyCallback,
                     impl_, std::move(callback)));
}

// static
bool NotificationPlatformBridgeWin::HandleActivation(
    const base::CommandLine& command_line) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NotificationLaunchId launch_id(base::UTF16ToUTF8(
      command_line.GetSwitchValueNative(switches::kNotificationLaunchId)));
  if (!launch_id.is_valid()) {
    LogActivationStatus(ActivationStatus::ACTIVATION_INVALID_LAUNCH_ID);
    return false;
  }

  base::Optional<base::string16> reply;
  base::string16 inline_reply =
      command_line.GetSwitchValueNative(switches::kNotificationInlineReply);
  if (!inline_reply.empty())
    reply = inline_reply;

  NotificationCommon::Operation operation = launch_id.is_for_context_menu()
                                                ? NotificationCommon::SETTINGS
                                                : NotificationCommon::CLICK;

  ForwardNotificationOperationOnUiThread(
      operation, launch_id.notification_type(), launch_id.origin_url(),
      launch_id.notification_id(), launch_id.profile_id(),
      launch_id.incognito(), launch_id.button_index(), reply, /*by_user=*/true);

  LogActivationStatus(ActivationStatus::SUCCESS);
  return true;
}

// static
std::string NotificationPlatformBridgeWin::GetProfileIdFromLaunchId(
    const base::string16& launch_id_str) {
  NotificationLaunchId launch_id(base::UTF16ToUTF8(launch_id_str));
  if (!launch_id.is_valid())
    LogActivationStatus(ActivationStatus::GET_PROFILE_ID_INVALID_LAUNCH_ID);
  return launch_id.is_valid() ? launch_id.profile_id() : std::string();
}

// static
bool NotificationPlatformBridgeWin::NativeNotificationEnabled() {
  return base::win::GetVersion() >= base::win::VERSION_WIN10_RS1 &&
         base::FeatureList::IsEnabled(features::kNativeNotifications);
}

void NotificationPlatformBridgeWin::ForwardHandleEventForTesting(
    NotificationCommon::Operation operation,
    winui::Notifications::IToastNotification* notification,
    winui::Notifications::IToastActivatedEventArgs* args,
    const base::Optional<bool>& by_user) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NotificationPlatformBridgeWinImpl::ForwardHandleEventForTesting,
          impl_, operation, base::Unretained(notification),
          base::Unretained(args), by_user));
}

void NotificationPlatformBridgeWin::SetDisplayedNotificationsForTesting(
    std::vector<ABI::Windows::UI::Notifications::IToastNotification*>*
        notifications) {
  NotificationPlatformBridgeWinImpl::notifications_for_testing_ = notifications;
}

HRESULT NotificationPlatformBridgeWin::GetToastNotificationForTesting(
    const message_center::Notification& notification,
    const NotificationTemplateBuilder& notification_template_builder,
    winui::Notifications::IToastNotification** toast_notification) {
  return impl_->GetToastNotification(
      notification, notification_template_builder, "UnusedValue",
      /*incognito=*/false, toast_notification);
}
