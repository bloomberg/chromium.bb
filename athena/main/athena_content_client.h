// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_ATHENA_CONTENT_CLIENT_H_
#define ATHENA_MAIN_ATHENA_CONTENT_CLIENT_H_

#include "extensions/shell/common/shell_content_client.h"

namespace athena {

class AthenaContentClient : public extensions::ShellContentClient {
 public:
  AthenaContentClient();
  virtual ~AthenaContentClient();

 private:
  // extensions::ShellContentClient:
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaContentClient);
};

}  // namespace athena

#endif  // ATHENA_MAIN_ATHENA_CONTENT_CLIENT_H_
