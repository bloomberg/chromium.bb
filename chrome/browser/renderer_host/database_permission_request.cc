// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/database_permission_request.h"

#include "chrome/browser/chrome_thread.h"

DatabasePermissionRequest::DatabasePermissionRequest(
    const string16& origin,
    const string16& database_name,
    Task* on_allow,
    Task* on_block)
    : origin_(origin),
      database_name_(database_name),
      on_allow_(on_allow),
      on_block_(on_block) {
  DCHECK(on_allow_.get());
  DCHECK(on_block_.get());
}

DatabasePermissionRequest::~DatabasePermissionRequest() {
}

void DatabasePermissionRequest::RequestPermission() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE, NewRunnableMethod(
          this, &DatabasePermissionRequest::RequestPermissionUI));
}

void DatabasePermissionRequest::RequestPermissionUI() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  bool allow = false;  // TODO(jorlow/darin): Allow user to choose.

  Task* task = allow ? on_allow_.release() : on_block_.release();
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, task);
}
