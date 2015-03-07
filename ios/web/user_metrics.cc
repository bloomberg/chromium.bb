// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/user_metrics.h"

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/user_metrics.h"
#include "ios/web/public/web_thread.h"

namespace web {

void RecordAction(const base::UserMetricsAction& action) {
  if (!WebThread::CurrentlyOn(WebThread::UI)) {
    WebThread::PostTask(WebThread::UI, FROM_HERE,
                        base::Bind(&web::RecordAction, action));
    return;
  }

  base::RecordAction(action);
}

void RecordComputedAction(const std::string& action) {
  if (!WebThread::CurrentlyOn(WebThread::UI)) {
    WebThread::PostTask(WebThread::UI, FROM_HERE,
                        base::Bind(&web::RecordComputedAction, action));
    return;
  }

  base::RecordComputedAction(action);
}

}  // namespace web
