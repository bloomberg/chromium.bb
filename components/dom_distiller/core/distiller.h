// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_

#include <map>

#include "base/callback.h"
#include "base/values.h"
#include "url/gurl.h"

namespace dom_distiller {

class DistilledPageProto;

class DistillerInterface {
 public:
  typedef base::Callback<void(DistilledPageProto*)> DistillerCallback;
  virtual ~DistillerInterface() {}

  // Distills a page, and asynchronously returns the article HTML to the
  // supplied callback.
  virtual void DistillPage(const GURL& url,
                           const DistillerCallback& callback) = 0;
};

class DistillerFactory {
 public:
  virtual ~DistillerFactory() {};
  virtual scoped_ptr<DistillerInterface> CreateDistiller() = 0;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
