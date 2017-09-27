// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_TAB_SOCKET_TEST_H_
#define HEADLESS_TEST_TAB_SOCKET_TEST_H_

#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_tab_socket.h"
#include "headless/test/headless_browser_test.h"

namespace headless {

class TabSocketTest : public HeadlessAsyncDevTooledBrowserTest,
                      public HeadlessTabSocket::Listener,
                      public page::Observer,
                      public runtime::Observer {
 public:
  TabSocketTest();
  ~TabSocketTest() override;

  // HeadlessAsyncDevTooledBrowserTest implementation:
  void SetUp() override;
  void RunDevTooledTest() override;
  bool GetAllowTabSockets() override;

  // runtime::Observer implementation:
  void OnExecutionContextCreated(
      const runtime::ExecutionContextCreatedParams& params) override;

  void ExpectJsException(std::unique_ptr<runtime::EvaluateResult> result);

  void FailOnJsEvaluateException(
      std::unique_ptr<runtime::EvaluateResult> result);

  // Note this will fail the test if an execution context corresponding to
  // |name| doesn't exist.
  int GetV8ExecutionContextIdByWorldName(const std::string& name);

  // Returns a pointer to the set of v8 execution context ids corresponding to
  // |devtools_frame_id| or null if none exist.
  const std::set<int>* GetV8ExecutionContextIdsForFrame(
      const std::string& devtools_frame_id) const;

  // Attempts to install a main world TabSocket in |devtools_frame_id|.
  // If successful |callback| will run with the execution context id of the
  // main world tab socket.
  void CreateMainWorldTabSocket(std::string devtools_frame_id,
                                base::Callback<void(int)> callback);

  // Attempts to create an isolated world in |isolated_world_name| and then
  // install a world TabSocket. If successful |callback| will run with the
  // execution context id of the newly created isolated world as a parameter.
  // Note |isolated_world_name| must be unique.
  void CreateIsolatedWorldTabSocket(std::string isolated_world_name,
                                    std::string devtools_frame_id,
                                    base::Callback<void(int)> callback);

  virtual void RunTabSocketTest() = 0;

  const std::string& main_frame_id() const { return *main_frame_id_; }

 private:
  void OnPageEnabled(std::unique_ptr<page::EnableResult> result);

  void OnResourceTree(std::unique_ptr<page::GetResourceTreeResult> result);

  void CreateMainWorldTabSocketStep2(std::string devtools_frame_id,
                                     base::Callback<void(int)> callback,
                                     int v8_execution_context_id);

  void OnCreatedIsolatedWorld(
      base::Callback<void(int)> callback,
      std::unique_ptr<page::CreateIsolatedWorldResult> result);

  void InstallHeadlessTabSocketBindings(base::Callback<void(int)> callback,
                                        int execution_context_id);

  void OnInstalledHeadlessTabSocketBindings(int execution_context_id,
                                            base::Callback<void(int)> callback,
                                            bool success);

  std::map<std::string, int> world_name_to_v8_execution_context_id_;
  std::map<std::string, std::set<int>> frame_id_to_v8_execution_context_ids_;
  base::Optional<std::string> main_frame_id_;
};

}  // namespace headless

#endif  // HEADLESS_TEST_TAB_SOCKET_TEST_H_
