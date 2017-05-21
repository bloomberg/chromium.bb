@echo off
:: Copyright 2013 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.
setlocal

:: Defer control.
%~dp0python "%~dp0\gn.py" %*
