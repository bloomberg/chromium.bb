// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_CLIENT_CONFIG_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_CLIENT_CONFIG_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "components/update_client/configurator.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace extensions {

// Used to provide configuration settings to the UpdateClient.
class UpdateClientConfig : public update_client::Configurator {
 public:
  UpdateClientConfig();

  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetSingleThreadTaskRunner()
      const override;

 protected:
  friend class base::RefCountedThreadSafe<UpdateClientConfig>;
  ~UpdateClientConfig() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateClientConfig);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_CLIENT_CONFIG_H_
