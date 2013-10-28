// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_

#include "base/memory/ref_counted.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/webrtc_audio_private.h"
#include "content/public/browser/render_view_host.h"
#include "media/audio/audio_device_name.h"
#include "url/gurl.h"

namespace base {
class MessageLoopProxy;
}

namespace extensions {

// Listens for device changes and forwards as an extension event.
class WebrtcAudioPrivateEventService
    : public ProfileKeyedAPI,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  explicit WebrtcAudioPrivateEventService(Profile* profile);
  virtual ~WebrtcAudioPrivateEventService();

  // ProfileKeyedAPI implementation.
  virtual void Shutdown() OVERRIDE;
  static ProfileKeyedAPIFactory<WebrtcAudioPrivateEventService>*
      GetFactoryInstance();
  static const char* service_name();

  // base::SystemMonitor::DevicesChangedObserver implementation.
  virtual void OnDevicesChanged(
      base::SystemMonitor::DeviceType device_type) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<WebrtcAudioPrivateEventService>;

  void SignalEvent();

  Profile* profile_;
};

class WebrtcAudioPrivateGetSinksFunction : public ChromeAsyncExtensionFunction {
 protected:
  virtual ~WebrtcAudioPrivateGetSinksFunction() {}

 private:
  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.getSinks",
                             WEBRTC_AUDIO_PRIVATE_GET_SINKS);

  virtual bool RunImpl() OVERRIDE;
  void DoQuery();
  void DoneOnUIThread();
};

// Common base for functions that start by retrieving the list of
// controllers for the specified tab.
class WebrtcAudioPrivateTabIdFunction : public ChromeAsyncExtensionFunction {
 protected:
  virtual ~WebrtcAudioPrivateTabIdFunction() {}

 protected:
  bool DoRunImpl(int tab_id);
  virtual void OnControllerList(
      const content::RenderViewHost::AudioOutputControllerList& list) = 0;
};

class WebrtcAudioPrivateGetActiveSinkFunction
    : public WebrtcAudioPrivateTabIdFunction {
 protected:
  virtual ~WebrtcAudioPrivateGetActiveSinkFunction() {}

 private:
  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.getActiveSink",
                             WEBRTC_AUDIO_PRIVATE_GET_ACTIVE_SINK);

  virtual bool RunImpl() OVERRIDE;
  virtual void OnControllerList(
      const content::RenderViewHost::AudioOutputControllerList&
      controllers) OVERRIDE;
  void OnSinkId(const std::string&);
};

class WebrtcAudioPrivateSetActiveSinkFunction
    : public WebrtcAudioPrivateTabIdFunction {
 public:
  WebrtcAudioPrivateSetActiveSinkFunction();

 protected:
  virtual ~WebrtcAudioPrivateSetActiveSinkFunction();

 private:
  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.setActiveSink",
                             WEBRTC_AUDIO_PRIVATE_SET_ACTIVE_SINK);

  virtual bool RunImpl() OVERRIDE;
  virtual void OnControllerList(
      const content::RenderViewHost::AudioOutputControllerList&
      controllers) OVERRIDE;
  void SwitchDone();
  void DoneOnUIThread();

  // Message loop of the thread this class is constructed on.
  const scoped_refptr<base::MessageLoopProxy> message_loop_;

  int tab_id_;
  std::string sink_id_;

  // Number of sink IDs we are still waiting for. Can become greater
  // than 0 in OnControllerList, decreases on every OnSinkId call.
  size_t num_remaining_sink_ids_;
};

class WebrtcAudioPrivateGetAssociatedSinkFunction
    : public ChromeAsyncExtensionFunction {
 protected:
  virtual ~WebrtcAudioPrivateGetAssociatedSinkFunction();

 private:
  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.getAssociatedSink",
                             WEBRTC_AUDIO_PRIVATE_GET_ASSOCIATED_SINK);

  virtual bool RunImpl() OVERRIDE;

  // Takes the parameters of the function, returns the associated sink
  // ID, or the empty string if none.
  std::string DoWorkOnDeviceThread(GURL security_origin,
                                   std::string source_id_in_origin);
  void DoneOnUIThread(const std::string& associated_sink_id);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
