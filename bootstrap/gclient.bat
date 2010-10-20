@echo off
:: Copyright (c) 2009 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: Synchronize the root directory before deferring control back up to it.
call "%~dp0\update_depot_tools.bat"

:: Defer control.
python "%~dp0\..\gclient.py" %*
