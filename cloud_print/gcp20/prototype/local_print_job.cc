// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/local_print_job.h"

LocalPrintJob::LocalPrintJob() : offline(false) {
}

LocalPrintJob::~LocalPrintJob() {
}

LocalPrintJob::Info::Info() : state(STATE_DRAFT), expires_in(-1) {
}

LocalPrintJob::Info::~Info() {
}

