// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SUGGESTION_H_
#define CHROME_BROWSER_VR_ELEMENTS_SUGGESTION_H_

#include "chrome/browser/vr/elements/linear_layout.h"
#include "url/gurl.h"

namespace vr {

// This class is a wrapper to assist in base::Bind operations. A suggestion must
// bind to both a URL and a browser interface to trigger navigations. Attempting
// to bind both necessitates lambda capture, which base::Bind cannot do. This
// workaround simplifies the problem. If UI elements gain the ability to use the
// VR UI to browser interface, we could eliminate this class.
class Suggestion : public LinearLayout {
 public:
  explicit Suggestion(base::Callback<void(GURL)> navigate_callback);
  ~Suggestion() override;

  void set_destination(GURL gurl) { destination_ = gurl; }

 private:
  void HandleButtonClick();

  base::Callback<void(GURL)> navigate_callback_;
  GURL destination_;

  DISALLOW_COPY_AND_ASSIGN(Suggestion);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SUGGESTION_H_
