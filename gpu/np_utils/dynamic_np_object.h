// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_NP_UTILS_DYNAMIC_NP_OBJECT_H_
#define GPU_NP_UTILS_DYNAMIC_NP_OBJECT_H_

#include <map>

#include "gpu/np_utils/default_np_object.h"
#include "gpu/np_utils/np_utils.h"

namespace np_utils {

// NPObjects of this type have a dictionary of property name / variant pairs
// that can be changed at runtime through NPAPI.
class DynamicNPObject : public DefaultNPObject<NPObject> {
 public:
  explicit DynamicNPObject(NPP npp);

  void Invalidate();
  bool HasProperty(NPIdentifier name);
  bool GetProperty(NPIdentifier name, NPVariant* result);
  bool SetProperty(NPIdentifier name, const NPVariant* value);
  bool RemoveProperty(NPIdentifier name);
  bool Enumerate(NPIdentifier** names, uint32_t* count);

 private:
  typedef std::map<NPIdentifier, SmartNPVariant> PropertyMap;
  PropertyMap properties_;
  DISALLOW_COPY_AND_ASSIGN(DynamicNPObject);
};
}  // namespace np_utils

#endif  // GPU_NP_UTILS_DYNAMIC_NP_OBJECT_H_
