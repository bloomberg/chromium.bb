// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_UPDATE_CHECKER_H_
#define COMPONENTS_COMPONENT_UPDATER_UPDATE_CHECKER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "components/component_updater/update_response.h"

class GURL;

namespace net {
class URLRequestContextGetter;
}

namespace component_updater {

class Configurator;
struct CrxUpdateItem;

class UpdateChecker {
 public:
  typedef base::Callback<void(int error,
                              const std::string& error_message,
                              const UpdateResponse::Results& results)>
      UpdateCheckCallback;

  virtual ~UpdateChecker() {}

  // Initiates an update check for the |items_to_check|. |additional_attributes|
  // provides a way to customize the <request> element. This value is inserted
  // as-is, therefore it must be well-formed as an XML attribute string.
  virtual bool CheckForUpdates(
      const std::vector<CrxUpdateItem*>& items_to_check,
      const std::string& additional_attributes) = 0;

  static scoped_ptr<UpdateChecker> Create(
      const Configurator& config,
      const UpdateCheckCallback& update_check_callback);

 protected:
  UpdateChecker() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateChecker);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_UPDATE_CHECKER_H_
