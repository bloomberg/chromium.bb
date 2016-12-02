@echo off
:: Copyright (c) 2016 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

powershell -NoProfile -ExecutionPolicy RemoteSigned -File "%~dp0\cipd.ps1" %*
