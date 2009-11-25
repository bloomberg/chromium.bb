// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gpu_plugin/gpu_plugin_object_factory.h"
#include "base/logging.h"

namespace np_utils {

NPPluginObjectFactory* NPPluginObjectFactory::factory_;

PluginObject* NPPluginObjectFactory::CreatePluginObject(
    NPP npp,
    NPMIMEType plugin_type) {
  return NULL;
}

NPPluginObjectFactory::NPPluginObjectFactory() {
  // Make this the first factory in the linked list.
  previous_factory_ = factory_;
  factory_ = this;
}

NPPluginObjectFactory::~NPPluginObjectFactory() {
  // Remove this factory from the linked list.
  DCHECK(factory_ == this);
  factory_ = previous_factory_;
}

}  // namespace np_utils
