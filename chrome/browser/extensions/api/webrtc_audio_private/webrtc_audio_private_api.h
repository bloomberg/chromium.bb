// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/webrtc_audio_private.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "media/audio/audio_device_description.h"

namespace media {
class AudioSystem;
}

namespace extensions {

// Listens for device changes and forwards as an extension event.
class WebrtcAudioPrivateEventService
    : public BrowserContextKeyedAPI,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  explicit WebrtcAudioPrivateEventService(content::BrowserContext* context);
  ~WebrtcAudioPrivateEventService() override;

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static BrowserContextKeyedAPIFactory<WebrtcAudioPrivateEventService>*
      GetFactoryInstance();
  static const char* service_name();

  // base::SystemMonitor::DevicesChangedObserver implementation.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

 private:
  friend class BrowserContextKeyedAPIFactory<WebrtcAudioPrivateEventService>;

  void SignalEvent();

  content::BrowserContext* browser_context_;
};

// Common base for WebrtcAudioPrivate functions, that provides a
// couple of optionally-used common implementations.
class WebrtcAudioPrivateFunction : public ChromeAsyncExtensionFunction {
 protected:
  WebrtcAudioPrivateFunction();
  ~WebrtcAudioPrivateFunction() override;

 protected:
  // Calculates a single HMAC, using the extension ID as the security origin.
  // Call only on IO thread.
  std::string CalculateHMAC(const std::string& raw_id);

  // Initializes |device_id_salt_|. Must be called on the UI thread,
  // before any calls to |device_id_salt()|.
  void InitDeviceIDSalt();

  // Callable from any thread. Must previously have called
  // |InitDeviceIDSalt()|.
  std::string device_id_salt() const;

  media::AudioSystem* GetAudioSystem();

  // Returns the RenderProcessHost associated with the given |request|
  // authorized by the |security_origin|. Returns null if unauthorized or
  // the RPH does not exist.
  content::RenderProcessHost* GetRenderProcessHostFromRequest(
      const api::webrtc_audio_private::RequestInfo& request,
      const std::string& security_origin);

 private:
  std::string device_id_salt_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcAudioPrivateFunction);
};

class WebrtcAudioPrivateGetSinksFunction : public WebrtcAudioPrivateFunction {
 protected:
  ~WebrtcAudioPrivateGetSinksFunction() override {}

 private:
  using SinkInfoVector = std::vector<api::webrtc_audio_private::SinkInfo>;

  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.getSinks",
                             WEBRTC_AUDIO_PRIVATE_GET_SINKS);

  bool RunAsync() override;

  // Requests output device descriptions.
  void GetOutputDeviceDescriptionsOnIOThread();

  // Receives output device descriptions, calculates HMACs for them and replies
  // to UI thread with DoneOnUIThread().
  void ReceiveOutputDeviceDescriptionsOnIOThread(
      media::AudioDeviceDescriptions sink_devices);

  // Sends the response.
  void DoneOnUIThread(std::unique_ptr<SinkInfoVector> results);
};

class WebrtcAudioPrivateGetAssociatedSinkFunction
    : public WebrtcAudioPrivateFunction {
 public:
  WebrtcAudioPrivateGetAssociatedSinkFunction();

 protected:
  ~WebrtcAudioPrivateGetAssociatedSinkFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.getAssociatedSink",
                             WEBRTC_AUDIO_PRIVATE_GET_ASSOCIATED_SINK);

  // UI thread: Entry point, posts GetInputDeviceDescriptions() to IO thread.
  bool RunAsync() override;

  // Enumerates input devices.
  void GetInputDeviceDescriptionsOnIOThread();

  // Receives the input device descriptions, looks up the raw source device ID
  // basing on |params|, and requests the associated raw sink ID for it.
  void ReceiveInputDeviceDescriptionsOnIOThread(
      media::AudioDeviceDescriptions source_devices);

  // IO thread: Receives the raw sink ID, calculates HMAC and replies to IO
  // thread with ReceiveHMACOnUIThread().
  void CalculateHMACOnIOThread(const base::Optional<std::string>& raw_sink_id);

  // Receives the associated sink ID as HMAC and sends the response.
  void ReceiveHMACOnUIThread(const std::string& hmac);

  // Initialized on UI thread in RunAsync(), read-only access on IO thread - no
  // locking needed.
  std::unique_ptr<api::webrtc_audio_private::GetAssociatedSink::Params> params_;
};

class WebrtcAudioPrivateSetAudioExperimentsFunction
    : public WebrtcAudioPrivateFunction {
 public:
  WebrtcAudioPrivateSetAudioExperimentsFunction();

 protected:
  ~WebrtcAudioPrivateSetAudioExperimentsFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.setAudioExperiments",
                             WEBRTC_AUDIO_PRIVATE_SET_AUDIO_EXPERIMENTS);

  bool RunAsync() override;

  // Must be called on the UI thread.
  void FireCallback(bool success, const std::string& error_message);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
