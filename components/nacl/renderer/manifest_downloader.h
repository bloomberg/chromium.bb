// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

namespace blink {
struct WebURLError;
class WebURLLoader;
class WebURLResponse;
}

namespace nacl {

// Downloads a NaCl manifest (.nmf) and returns the contents of the file to
// caller through a callback.
class ManifestDownloader : public blink::WebURLLoaderClient {
 public:
  typedef base::Callback<void(PP_NaClError, const std::string&)> Callback;

  // This is a pretty arbitrary limit on the byte size of the NaCl manifest
  // file.
  // Note that the resulting string object has to have at least one byte extra
  // for the null termination character.
  static const size_t kNaClManifestMaxFileBytes = 1024 * 1024;

  ManifestDownloader(scoped_ptr<blink::WebURLLoader> url_loader,
                     bool is_installed,
                     Callback cb);
  virtual ~ManifestDownloader();

  void Load(const blink::WebURLRequest& request);

 private:
  // WebURLLoaderClient implementation.
  virtual void didReceiveResponse(blink::WebURLLoader* loader,
                                  const blink::WebURLResponse& response);
  virtual void didReceiveData(blink::WebURLLoader* loader,
                              const char* data,
                              int data_length,
                              int encoded_data_length);
  virtual void didFinishLoading(blink::WebURLLoader* loader,
                                double finish_time,
                                int64_t total_encoded_data_length);
  virtual void didFail(blink::WebURLLoader* loader,
                       const blink::WebURLError& error);

  scoped_ptr<blink::WebURLLoader> url_loader_;
  bool is_installed_;
  Callback cb_;
  std::string buffer_;
  int status_code_;
  PP_NaClError pp_nacl_error_;
};

}  // namespace nacl
