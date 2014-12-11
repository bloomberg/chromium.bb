// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_SEND_COMMAND_REQUEST_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_SEND_COMMAND_REQUEST_H_

#include "chrome/browser/local_discovery/gcd_api_flow.h"

class SendCommandRequest : public local_discovery::GCDApiFlowRequest {
 public:
  class Delegate {
   public:
    // It's safe to destroy SendCommandRequest in these methods.
    virtual void OnCommandSucceeded(const base::DictionaryValue& value) = 0;
    virtual void OnCommandFailed() = 0;

   protected:
    ~Delegate() {}
  };

  SendCommandRequest(const base::DictionaryValue* request, Delegate* delegate);

  // Implementation of GCDApiFlowRequest.
  net::URLFetcher::RequestType GetRequestType() override;
  void GetUploadData(std::string* upload_type,
                     std::string* upload_data) override;
  void OnGCDAPIFlowError(local_discovery::GCDApiFlow::Status status) override;
  void OnGCDAPIFlowComplete(const base::DictionaryValue& value) override;
  GURL GetURL() override;

 private:
  std::string upload_data_;
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(SendCommandRequest);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_SEND_COMMAND_REQUEST_H_
