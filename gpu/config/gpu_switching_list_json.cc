// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Determines whether a certain gpu is prefered in a dual-gpu situation.
// The format of a valid gpu_switching_list.json file is defined in
// <gpu/config/gpu_control_list_format.txt>.
// The supported "features" can be found in <gpu/config/gpu_switching_list.cc>.

#include "gpu/config/gpu_control_list_jsons.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace gpu {

const char kGpuSwitchingListJson[] = LONG_STRING_CONST(

{
  "name": "gpu switching list",
  // Please update the version number whenever you change this file.
  "version": "2.0",
  "entries": [
    {
      "id": 1,
      "description": "Force to use discrete GPU on older MacBookPro models.",
      "cr_bugs": [113703],
      "os": {
        "type": "macosx",
        "version": {
          "op": ">=",
          "value": "10.7"
        }
      },
      "machine_model": {
        "name": {
          "op": "=",
          "value": "MacBookPro"
        },
        "version": {
          "op": "<",
          "value": "8"
        }
      },
      "gpu_count": {
        "op": "=",
        "value": "2"
      },
      "features": [
        "force_discrete"
      ]
    }
  ]
}

);  // LONG_STRING_CONST macro

}  // namespace gpu

