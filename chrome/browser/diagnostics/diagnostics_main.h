// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MAIN_H_
#define CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MAIN_H_
#pragma once

class CommandLine;

// Entry point for the diagnostics mode. Most of the initialization that you
// can see in ChromeMain() will be repeated here or will be done differently.
int DiagnosticsMain(const CommandLine& command_line);


#endif  // CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MAIN_H_
