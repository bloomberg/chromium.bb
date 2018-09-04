// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace autofill_assistant {
// Autofill assistant service to communicate with the server to get scripts and
// client actions.
class Service {
 public:
  explicit Service(const std::string& api_key,
                   content::BrowserContext* context);
  virtual ~Service();

  using ResponseCallback =
      base::OnceCallback<void(bool result, const std::string&)>;
  // Get scripts for a given |url|, which should be a valid URL.
  virtual void GetScriptsForUrl(const GURL& url, ResponseCallback callback);

  // Get actions.
  virtual void GetActions(const std::string& script_path,
                          ResponseCallback callback);

  // Get next sequence of actions according to server payload in previous
  // response.
  virtual void GetNextActions(
      const std::string& previous_server_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback);

 private:
  // Struct to store scripts and actions request.
  struct Loader {
    Loader();
    ~Loader();

    ResponseCallback callback;
    std::unique_ptr<::network::SimpleURLLoader> loader;
  };
  std::unique_ptr<::network::SimpleURLLoader> CreateAndStartLoader(
      const GURL& server_url,
      const std::string& request,
      Loader* loader);
  void OnURLLoaderComplete(Loader* loader,
                           std::unique_ptr<std::string> response_body);

  content::BrowserContext* context_;
  GURL script_server_url_;
  GURL script_action_server_url_;

  // Destroying this object will cancel ongoing requests.
  std::map<Loader*, std::unique_ptr<Loader>> loaders_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_H_
