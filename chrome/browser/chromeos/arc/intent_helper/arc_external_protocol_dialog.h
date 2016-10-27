// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_EXTERNAL_PROTOCOL_DIALOG_H_

#include <string>
#include <utility>

#include "components/arc/common/intent_helper.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace arc {

enum class GetActionResult {
  SHOW_CHROME_OS_DIALOG,
  OPEN_URL_IN_CHROME,
  HANDLE_URL_IN_ARC,
  ASK_USER,
};

// Shows ARC version of the dialog. Returns true if ARC is supported, running,
// and in a context where it is allowed to handle external protocol.
bool RunArcExternalProtocolDialog(const GURL& url,
                                  int render_process_host_id,
                                  int routing_id,
                                  ui::PageTransition page_transition,
                                  bool has_user_gesture);

bool ShouldIgnoreNavigationForTesting(ui::PageTransition page_transition);

GetActionResult GetActionForTesting(
    const GURL& original_url,
    const mojo::Array<mojom::IntentHandlerInfoPtr>& handlers,
    size_t selected_app_index,
    std::pair<GURL, std::string>* out_url_and_package);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_EXTERNAL_PROTOCOL_DIALOG_H_
