// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_IO_CONTEXT_
#define CHROME_BROWSER_INSTANT_INSTANT_IO_CONTEXT_

#include <set>

#include "base/basictypes.h"
#include "base/supports_user_data.h"

namespace content {
class ResourceContext;
}

namespace net {
class URLRequest;
}

// IO thread data held for Instant.  This reflects the data held in
// InstantService for use on the IO thread.  Owned by ResourceContext
// as user data.
class InstantIOContext : public base::SupportsUserData::Data {
 public:
  InstantIOContext();
  virtual ~InstantIOContext();

  // Add and remove RenderProcessHost IDs that are associated with Instant
  // processes.  Used to keep process IDs in sync with InstantService.
  static void AddInstantProcessOnIO(content::ResourceContext* context,
                                    int process_id);
  static void RemoveInstantProcessOnIO(content::ResourceContext* context,
                                       int process_id);
  static void ClearInstantProcessesOnIO(content::ResourceContext* context);

  // Determine if this chrome-search: request is coming from an Instant render
  // process.
  static bool ShouldServiceRequest(const net::URLRequest* request);

 private:
  // Check that |process_id| is in the known set of Instant processes, ie.
  // |process_ids_|.
  bool IsInstantProcess(int process_id) const;

  // The process IDs associated with Instant processes.  Mirror of the process
  // IDs in InstantService.  Duplicated here for synchronous access on the IO
  // thread.
  std::set<int> process_ids_;

  DISALLOW_COPY_AND_ASSIGN(InstantIOContext);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_IO_CONTEXT_
