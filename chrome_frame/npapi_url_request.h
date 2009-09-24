// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_NPAPI_URL_REQUEST_H_
#define CHROME_FRAME_NPAPI_URL_REQUEST_H_

#include "base/platform_thread.h"
#include "chrome_frame/plugin_url_request.h"
#include "third_party/WebKit/WebCore/bridge/npapi.h"

class NPAPIUrlRequest : public PluginUrlRequest {
 public:
  explicit NPAPIUrlRequest(NPP instance);
  ~NPAPIUrlRequest();

  virtual bool Start();
  virtual void Stop();
  virtual bool Read(int bytes_to_read);

  // Called from NPAPI
  bool OnStreamCreated(const char* mime_type, NPStream* stream);
  void OnStreamDestroyed(NPReason reason);
  int OnWriteReady();
  int OnWrite(void* buffer, int len);

  // Thread unsafe implementation of ref counting, since
  // this will be called on the plugin UI thread only.
  virtual unsigned long API_CALL AddRef();
  virtual unsigned long API_CALL Release();

 private:
  unsigned long ref_count_;
  NPP instance_;
  NPStream* stream_;
  size_t pending_read_size_;
  URLRequestStatus status_;

  PlatformThreadId thread_;
  static int instance_count_;
  DISALLOW_COPY_AND_ASSIGN(NPAPIUrlRequest);
};

#endif  // CHROME_FRAME_NPAPI_URL_REQUEST_H_

