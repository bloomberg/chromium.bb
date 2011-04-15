// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_BROWSER_RESOURCE_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace webkit_database {
class DatabaseTracker;
}  // namespace webkit_database

namespace content {

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread. ResourceContext doesn't own anything it points to, it just
// holds pointers to relevant objects to resource loading.
class ResourceContext {
 public:
  virtual ~ResourceContext();

  webkit_database::DatabaseTracker* database_tracker() const;
  void set_database_tracker(webkit_database::DatabaseTracker* tracker);

 protected:
  ResourceContext();

 private:
  virtual void EnsureInitialized() const = 0;

  webkit_database::DatabaseTracker* database_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ResourceContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RESOURCE_CONTEXT_H_
