// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CHROME_WEBRTC_INTERNALS_H_
#define CHROME_BROWSER_MEDIA_CHROME_WEBRTC_INTERNALS_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/process.h"
#include "base/values.h"
#include "content/public/browser/webrtc_internals.h"

class WebRTCInternalsUIObserver;

// This is a singleton class running in the browser UI thread.
// It collects peer connection infomation from the renderers,
// forwards the data to WebRTCInternalsUIObserver and
// sends data collecting commands to the renderers.
class ChromeWebRTCInternals : public content::WebRTCInternals {
 public:
  static ChromeWebRTCInternals* GetInstance();

  virtual void AddPeerConnection(base::ProcessId pid,
                                 int lid,
                                 const std::string& url,
                                 const std::string& servers,
                                 const std::string& constraints) OVERRIDE;
  virtual void RemovePeerConnection(base::ProcessId pid, int lid) OVERRIDE;

  // Methods for adding or removing WebRTCInternalsUIObserver.
  void AddObserver(WebRTCInternalsUIObserver *observer);
  void RemoveObserver(WebRTCInternalsUIObserver *observer);

 private:
  friend struct DefaultSingletonTraits<ChromeWebRTCInternals>;

  ChromeWebRTCInternals();
  virtual ~ChromeWebRTCInternals();

  void SendUpdate(const std::string& command, base::Value* value);

  ObserverList<WebRTCInternalsUIObserver> observers_;
  base::ListValue peer_connection_data_;
};

#endif  // CHROME_BROWSER_MEDIA_CHROME_WEBRTC_INTERNALS_H_
