// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_util.h"

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>

// Helper function to call the completion callback if it is defined (an
// asynchronous call), or return the result directly if it isn't (a synchronous
// call).
//
// Use like this:
//   if (<some error condition>)
//     return RunCompletionCallback(callback, PP_ERROR_FUBAR);
//
//   /* Everything worked OK */
//   return RunCompletionCallback(callback, PP_OK);
int32_t RunCompletionCallback(PP_CompletionCallback* callback, int32_t result) {
  if (callback->func) {
    PP_RunCompletionCallback(callback, result);
    return PP_OK_COMPLETIONPENDING;
  }
  return result;
}
