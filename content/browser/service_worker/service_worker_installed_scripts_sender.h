// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPTS_SENDER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPTS_SENDER_H_

#include "content/common/service_worker/service_worker_installed_scripts_manager.mojom.h"

namespace content {

struct HttpResponseInfoIOBuffer;
class ServiceWorkerVersion;

// ServiceWorkerInstalledScriptsSender serves the service worker's installed
// scripts from ServiceWorkerStorage to the renderer through Mojo data pipes.
// ServiceWorkerInstalledScriptsSender is owned by ServiceWorkerVersion. It is
// created for worker startup and lives as long as the worker is running.
class CONTENT_EXPORT ServiceWorkerInstalledScriptsSender {
 public:
  // Do not change the order. This is used for UMA.
  enum class FinishedReason {
    kNotFinished = 0,
    kSuccess = 1,
    kNoHttpInfoError = 2,
    kCreateDataPipeError = 3,
    kConnectionError = 4,
    kResponseReaderError = 5,
    kMetaDataSenderError = 6,
    // Add a new type here, then update kMaxValue and enums.xml.
    kMaxValue = kMetaDataSenderError,
  };

  // |owner| must be an installed service worker.
  explicit ServiceWorkerInstalledScriptsSender(ServiceWorkerVersion* owner);

  ~ServiceWorkerInstalledScriptsSender();

  // Creates a Mojo struct (mojom::ServiceWorkerInstalledScriptsInfo) and sets
  // it with the information to create WebServiceWorkerInstalledScriptsManager
  // on the renderer.
  mojom::ServiceWorkerInstalledScriptsInfoPtr CreateInfoAndBind();

  // Starts sending installed scripts to the worker.
  void Start();

  bool IsFinished() const;
  FinishedReason finished_reason() const { return finished_reason_; }

 private:
  class Sender;

  enum class State {
    kNotStarted,
    kSendingMainScript,
    kSendingImportedScript,
    kFinished,
  };

  void StartSendingScript(int64_t resource_id);

  // Called from |running_sender_|.
  void SendScriptInfoToRenderer(
      std::string encoding,
      std::unordered_map<std::string, std::string> headers,
      mojo::ScopedDataPipeConsumerHandle body_handle,
      uint64_t body_size,
      mojo::ScopedDataPipeConsumerHandle meta_data_handle,
      uint64_t meta_data_size);
  void OnHttpInfoRead(scoped_refptr<HttpResponseInfoIOBuffer> http_info);
  void OnFinishSendingScript();
  void OnAbortSendingScript(FinishedReason reason);

  const GURL& CurrentSendingURL();

  void UpdateState(State state);
  void Finish(FinishedReason reason);

  ServiceWorkerVersion* owner_;
  const GURL main_script_url_;
  const int64_t main_script_id_;

  mojom::ServiceWorkerInstalledScriptsManagerPtr manager_;
  std::unique_ptr<Sender> running_sender_;
  State state_;
  FinishedReason finished_reason_;
  std::map<int64_t /* resource_id */, GURL> imported_scripts_;
  std::map<int64_t /* resource_id */, GURL>::iterator imported_script_iter_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerInstalledScriptsSender);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPTS_SENDER_H_
