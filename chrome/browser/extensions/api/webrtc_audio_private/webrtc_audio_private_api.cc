// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_controller.h"

namespace extensions {

using content::BrowserThread;
using content::RenderViewHost;
using media::AudioDeviceNames;
using media::AudioManager;

namespace wap = api::webrtc_audio_private;

static base::LazyInstance<
    ProfileKeyedAPIFactory<WebrtcAudioPrivateEventService> > g_factory =
        LAZY_INSTANCE_INITIALIZER;

WebrtcAudioPrivateEventService::WebrtcAudioPrivateEventService(
    Profile* profile) : profile_(profile) {
  // In unit tests, the SystemMonitor may not be created.
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->AddDevicesChangedObserver(this);
}

WebrtcAudioPrivateEventService::~WebrtcAudioPrivateEventService() {
}

void WebrtcAudioPrivateEventService::Shutdown() {
  // In unit tests, the SystemMonitor may not be created.
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveDevicesChangedObserver(this);
}

// static
ProfileKeyedAPIFactory<WebrtcAudioPrivateEventService>*
WebrtcAudioPrivateEventService::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
const char* WebrtcAudioPrivateEventService::service_name() {
  return "WebrtcAudioPrivateEventService";
}

void WebrtcAudioPrivateEventService::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE:
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      SignalEvent();
      break;

    default:
      // No action needed.
      break;
  }
}

void WebrtcAudioPrivateEventService::SignalEvent() {
  using api::webrtc_audio_private::OnSinksChanged::kEventName;

  EventRouter* router =
      ExtensionSystem::Get(profile_)->event_router();
  if (!router || !router->HasEventListener(kEventName))
    return;
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  const ExtensionSet* extensions = extension_service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const std::string& extension_id = (*it)->id();
    if (router->ExtensionHasEventListener(extension_id, kEventName) &&
        (*it)->HasAPIPermission("webrtcAudioPrivate")) {
      scoped_ptr<Event> event(
          new Event(kEventName, make_scoped_ptr(new ListValue()).Pass()));
      router->DispatchEventToExtension(extension_id, event.Pass());
    }
  }
}

bool WebrtcAudioPrivateGetSinksFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AudioManager::Get()->GetMessageLoop()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WebrtcAudioPrivateGetSinksFunction::DoQuery, this),
      base::Bind(&WebrtcAudioPrivateGetSinksFunction::DoneOnUIThread, this));
  return true;
}

void WebrtcAudioPrivateGetSinksFunction::DoQuery() {
  DCHECK(AudioManager::Get()->GetMessageLoop()->BelongsToCurrentThread());

  AudioDeviceNames device_names;
  AudioManager::Get()->GetAudioOutputDeviceNames(&device_names);

  std::vector<linked_ptr<wap::SinkInfo> > results;
  for (AudioDeviceNames::const_iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    linked_ptr<wap::SinkInfo> info(new wap::SinkInfo);
    info->sink_id = it->unique_id;
    info->sink_label = it->device_name;
    // TODO(joi): Add other parameters.
    results.push_back(info);
  }

  // It's safe to directly set the results here (from a thread other
  // than the UI thread, on which an AsyncExtensionFunction otherwise
  // normally runs) because there is one instance of this object per
  // function call, no actor outside of this object is modifying the
  // results_ member, and the different method invocations on this
  // object run strictly in sequence; first RunImpl on the UI thread,
  // then DoQuery on the audio IO thread, then DoneOnUIThread on the
  // UI thread.
  SetResult(wap::GetSinks::Results::Create(results).release());
}

void WebrtcAudioPrivateGetSinksFunction::DoneOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

bool WebrtcAudioPrivateTabIdFunction::DoRunImpl(int tab_id) {
  content::WebContents* contents = NULL;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile(), true,
                                    NULL, NULL, &contents, NULL)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return false;
  }

  RenderViewHost* rvh = contents->GetRenderViewHost();
  if (!rvh)
    return false;

  rvh->GetAudioOutputControllers(base::Bind(
      &WebrtcAudioPrivateTabIdFunction::OnControllerList, this));
  return true;
}

bool WebrtcAudioPrivateGetActiveSinkFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<wap::GetActiveSink::Params> params(
      wap::GetActiveSink::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return DoRunImpl(params->tab_id);
}

void WebrtcAudioPrivateGetActiveSinkFunction::OnControllerList(
    const RenderViewHost::AudioOutputControllerList& controllers) {
  if (controllers.empty()) {
    // If there is no current audio stream for the rvh, we return an
    // empty string as the sink ID.
    DVLOG(2) << "chrome.webrtcAudioPrivate.getActiveSink: No controllers.";
    SetResult(wap::GetActiveSink::Results::Create(std::string()).release());
    SendResponse(true);
  } else {
    DVLOG(2) << "chrome.webrtcAudioPrivate.getActiveSink: "
             << controllers.size() << " controllers.";
    // TODO(joi): Debug-only, DCHECK that all items have the same ID.
    (*controllers.begin())->GetOutputDeviceId(
        base::Bind(&WebrtcAudioPrivateGetActiveSinkFunction::OnSinkId, this));
  }
}

void WebrtcAudioPrivateGetActiveSinkFunction::OnSinkId(const std::string& id) {
  std::string result = id;
  if (result.empty()) {
    DVLOG(2) << "Received empty ID, replacing with default ID.";
    result = media::AudioManagerBase::kDefaultDeviceId;
  }
  SetResult(wap::GetActiveSink::Results::Create(result).release());
  SendResponse(true);
}

WebrtcAudioPrivateSetActiveSinkFunction::
WebrtcAudioPrivateSetActiveSinkFunction()
    : message_loop_(base::MessageLoopProxy::current()),
      tab_id_(0),
      num_remaining_sink_ids_(0) {
}

WebrtcAudioPrivateSetActiveSinkFunction::
~WebrtcAudioPrivateSetActiveSinkFunction() {
}

bool WebrtcAudioPrivateSetActiveSinkFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<wap::SetActiveSink::Params> params(
      wap::SetActiveSink::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tab_id_ = params->tab_id;
  sink_id_ = params->sink_id;

  return DoRunImpl(tab_id_);
}

void WebrtcAudioPrivateSetActiveSinkFunction::OnControllerList(
    const RenderViewHost::AudioOutputControllerList& controllers) {
  num_remaining_sink_ids_ = controllers.size();
  if (num_remaining_sink_ids_ == 0) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        "No active stream for tab with id: *.",
        base::IntToString(tab_id_));
    SendResponse(false);
  } else {
    RenderViewHost::AudioOutputControllerList::const_iterator it =
        controllers.begin();
    for (; it != controllers.end(); ++it) {
      (*it)->SwitchOutputDevice(sink_id_, base::Bind(
          &WebrtcAudioPrivateSetActiveSinkFunction::SwitchDone, this));
    }
  }
}

void WebrtcAudioPrivateSetActiveSinkFunction::SwitchDone() {
  if (--num_remaining_sink_ids_ == 0) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&WebrtcAudioPrivateSetActiveSinkFunction::DoneOnUIThread,
                   this));
  }
}

void WebrtcAudioPrivateSetActiveSinkFunction::DoneOnUIThread() {
  SendResponse(true);
}

WebrtcAudioPrivateGetAssociatedSinkFunction::
~WebrtcAudioPrivateGetAssociatedSinkFunction() {
}

bool WebrtcAudioPrivateGetAssociatedSinkFunction::RunImpl() {
  scoped_ptr<wap::GetAssociatedSink::Params> params(
      wap::GetAssociatedSink::Params::Create(*args_));
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::PostTaskAndReplyWithResult(
      AudioManager::Get()->GetMessageLoop(),
      FROM_HERE,
      base::Bind(
          &WebrtcAudioPrivateGetAssociatedSinkFunction::DoWorkOnDeviceThread,
          this, GURL(params->security_origin), params->source_id_in_origin),
      base::Bind(
          &WebrtcAudioPrivateGetAssociatedSinkFunction::DoneOnUIThread,
          this));

  return true;
}

std::string WebrtcAudioPrivateGetAssociatedSinkFunction::DoWorkOnDeviceThread(
    GURL security_origin, std::string source_id_in_origin) {
  AudioDeviceNames source_devices;
  AudioManager::Get()->GetAudioInputDeviceNames(&source_devices);

  // Find the raw source ID for source_id_in_origin.
  std::string raw_source_id;
  for (AudioDeviceNames::const_iterator it = source_devices.begin();
       it != source_devices.end();
       ++it) {
    const std::string& id = it->unique_id;
    if (content::DoesMediaDeviceIDMatchHMAC(
            security_origin, source_id_in_origin, id)) {
      raw_source_id = id;
      DVLOG(2) << "Found raw ID " << raw_source_id
               << " for source ID in origin " << source_id_in_origin;
      break;
    }
  }

  // We return an empty string if there is no associated output device.
  std::string result;
  if (!raw_source_id.empty()) {
    result = AudioManager::Get()->GetAssociatedOutputDeviceID(raw_source_id);
  }

  return result;
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::DoneOnUIThread(
    const std::string& associated_sink_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetResult(wap::GetAssociatedSink::Results::Create(
      associated_sink_id).release());
  SendResponse(true);
}

}  // namespace extensions
