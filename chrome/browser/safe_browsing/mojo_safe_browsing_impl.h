// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_MOJO_SAFE_BROWSING_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_MOJO_SAFE_BROWSING_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing_db/database_manager.h"
#include "ipc/ipc_message.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace safe_browsing {

// This class implements the Mojo interface for renderers to perform
// SafeBrowsing URL checks.
class MojoSafeBrowsingImpl : public mojom::SafeBrowsing {
 public:
  MojoSafeBrowsingImpl(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      int render_process_id);
  ~MojoSafeBrowsingImpl() override;

  static void Create(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      int render_process_id,
      const service_manager::BindSourceInfo& source_info,
      mojom::SafeBrowsingRequest request);

 private:
  // mojom::SafeBrowsing implementation.
  void CreateCheckerAndCheck(int32_t render_frame_id,
                             mojom::SafeBrowsingUrlCheckerRequest request,
                             const GURL& url,
                             int32_t load_flags,
                             content::ResourceType resource_type,
                             CreateCheckerAndCheckCallback callback) override;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  int render_process_id_ = MSG_ROUTING_NONE;

  DISALLOW_COPY_AND_ASSIGN(MojoSafeBrowsingImpl);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_MOJO_SAFE_BROWSING_IMPL_H_
