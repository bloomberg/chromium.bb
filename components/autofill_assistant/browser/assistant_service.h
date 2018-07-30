// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SERVICE_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace autofill_assistant {
// Autofill assistant service to communicate with the server to get scripts and
// client actions.
class AssistantService {
 public:
  AssistantService(content::BrowserContext* context);
  ~AssistantService();

  using ResponseCallback =
      base::OnceCallback<void(bool result, const std::string&)>;
  // Get assistant scripts for a given |url|, which should be a valid URL.
  void GetAssistantScriptsForUrl(const GURL& url, ResponseCallback callback);

  // Get assistant actions.
  void GetAssistantActions(const std::string& script_path,
                           ResponseCallback callback);

  // Get next sequence of assistant actions according to server payload in
  // previous reponse.
  void GetNextAssistantActions(const std::string& previous_server_payload,
                               ResponseCallback callback);

 private:
  // Struct to store assistant scripts and actions request.
  struct AssistantLoader {
    AssistantLoader();
    ~AssistantLoader();

    ResponseCallback callback;
    std::unique_ptr<network::SimpleURLLoader> loader;
  };
  std::unique_ptr<network::SimpleURLLoader> CreateAndStartLoader(
      const GURL& server_url,
      const std::string& request,
      AssistantLoader* loader);
  void OnURLLoaderComplete(AssistantLoader* loader,
                           std::unique_ptr<std::string> response_body);

  content::BrowserContext* context_;
  GURL assistant_script_server_url_;
  GURL assistant_script_action_server_url_;

  // Destroying this object will cancel ongoing requests.
  std::map<AssistantLoader*, std::unique_ptr<AssistantLoader>>
      assistant_loaders_;

  DISALLOW_COPY_AND_ASSIGN(AssistantService);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SERVICE_H_
