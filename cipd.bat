@echo off
:: Copyright (c) 2016 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: To allow this powershell script to run if it was a byproduct of downloading
:: and unzipping the depot_tools.zip distribution, we clear the Zone.Identifier
:: alternate data stream. This is equivalent to clicking the "Unblock" button
:: in the file's properties dialog.
echo.>"%~dp0\cipd.ps1:Zone.Identifier"

powershell -NoProfile -ExecutionPolicy RemoteSigned -Command "%~dp0\cipd.ps1" %*
