// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_CHECK_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_CHECK_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/version.h"
#include "components/update_client/action.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/update_response.h"
#include "url/gurl.h"

namespace update_client {

class UpdateChecker;

// Implements an update check for the CRXs in an update context.
class ActionUpdateCheck : public Action, private ActionImpl {
 public:
  ActionUpdateCheck(scoped_ptr<UpdateChecker> update_checker,
                    const base::Version& browser_version,
                    const std::string& extra_request_parameters);

  ~ActionUpdateCheck() override;

  void Run(UpdateContext* update_context, Callback callback) override;

 private:
  void UpdateCheckComplete(const GURL& original_url,
                           int error,
                           const std::string& error_message,
                           const UpdateResponse::Results& results);

  void OnUpdateCheckSucceeded(const UpdateResponse::Results& results);
  void OnUpdateCheckFailed(int error, const std::string& error_message);

  scoped_ptr<UpdateChecker> update_checker_;
  const base::Version browser_version_;
  const std::string extra_request_parameters_;

  DISALLOW_COPY_AND_ASSIGN(ActionUpdateCheck);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_CHECK_H_
