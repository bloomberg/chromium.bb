// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPS_GENERATE_STATE_JSON_H_
#define CHROME_BROWSER_CAPS_GENERATE_STATE_JSON_H_

#include "base/memory/ref_counted.h"

class TaskManagerModel;

namespace base {
class File;
}

namespace caps {

// Creates a JSON stream with the browser and system state and
// sends it to |file| which in practice is a pipe.
void GenerateStateJSON(
    scoped_refptr<TaskManagerModel> model, base::File file);

}

#endif  // CHROME_BROWSER_CAPS_GENERATE_STATE_JSON_H_
