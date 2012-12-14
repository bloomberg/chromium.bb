// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_infobar.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "ppapi/c/private/ppb_nacl_private.h"

using content::BrowserThread;

namespace {

void ShowInfobar(int render_process_id, int render_view_id,
                 int error_id) {
  // TODO(dschuff): hook this up to a real infobar in a future CL
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(static_cast<PP_NaClError>(error_id) ==
        PP_NACL_MANIFEST_MISSING_ARCH);
}

}  // namespace

void ShowNaClInfobar(int render_process_id, int render_view_id,
                     int error_id) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ShowInfobar, render_process_id, render_view_id,
                 error_id));
}
