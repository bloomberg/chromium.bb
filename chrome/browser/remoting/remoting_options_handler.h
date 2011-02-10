// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_REMOTING_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_REMOTING_REMOTING_OPTIONS_HANDLER_H_

#include "base/basictypes.h"
#include "chrome/browser/service/service_process_control.h"

class WebUI;

namespace remoting {

// Remoting options handler is responsinble for showing correct status
// of the chromoting host in the preferences. It listens to the
// messages from the service process (by registering MessageHandler
// callback in the ServiceProcessControl) and updates current status
// as neccessary.
class RemotingOptionsHandler : public ServiceProcessControl::MessageHandler {
 public:
  RemotingOptionsHandler();
  virtual ~RemotingOptionsHandler();

  void Init(WebUI* web_ui);

  // ServiceProcessControl::MessageHandler interface.
  virtual void OnRemotingHostInfo(
      const remoting::ChromotingHostInfo& host_info);

 private:
  void SetStatus(bool enabled, const std::string& login);

  WebUI* web_ui_;
  ServiceProcessControl* process_control_;

  DISALLOW_COPY_AND_ASSIGN(RemotingOptionsHandler);
};

}  // namespace remoting

#endif  // CHROME_BROWSER_REMOTING_REMOTING_OPTIONS_HANDLER_H_
