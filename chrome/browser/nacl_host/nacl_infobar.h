// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_H_
#define CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_H_

// Display an infobar to the user. The message_id corresponds to
// a PP_NaClError enum in ppb_nacl_private_impl.idl
void ShowNaClInfobar(int render_process_id, int render_view_id,
                     int error_id);

#endif  // CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_H_
