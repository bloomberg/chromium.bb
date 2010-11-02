@echo off
REM Copyright (c) 2010 The Chromium Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

IF EXIST "%~p0..\third_party\python_26\python.exe" GOTO usestandard

REM try the built-in Python if we can't find the "standard" Python.
echo Using system installed Python
python "%~p0tools\smoke_test.py" %*
GOTO end

:usestandard
"%~p0..\third_party\python_26\python.exe" "%~p0tools\smoke_test.py" %*

:end
