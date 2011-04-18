// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_
#pragma once

class Profile;

namespace first_run {

// Shows the search engine choice dialog. Only called if IsOrganicFirstRun()
// is true.
void ShowFirstRunDialog(Profile* profile,
                        bool randomize_search_engine_experiment);

}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_DIALOG_H_
