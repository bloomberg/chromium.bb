@echo off
:: Copyright (c) 2009 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This script will try to find if svn and python are accessible and it not,
:: it will try to download it and 'install' it in depot_tools.

SETLOCAL

set ROOT_URL=http://src.chromium.org/svn/trunk/tools
set ROOT_DIR=%~dp0..\..

:: If the batch file exists, skip the svn check.
if exist "%ROOT_DIR%\svn.bat" goto :PYTHON
call svn --version 2>nul 1>nul
if errorlevel 1 call :C_SVN

:PYTHON
:: If the batch file exists, skip the python check.
if exist "%ROOT_DIR%\python.bat" goto :EOF
call python --version 2>nul 1>nul
if errorlevel 1 call :C_PYTHON

:: We are done.
goto :EOF


:C_SVN
echo Installing subversion ...
:: svn is not accessible; check it out and create 'proxy' files.
call "%~dp0wget" -q %ROOT_URL%/third_party/svn.7z -O "%~dp0svn.7z"
if errorlevel 1 goto :SVN_FAIL
call "%~dp07za" x "%~dp0svn.7z" %ROOT_DIR%
if errorlevel 1 goto :SVN_FAIL
del "%~dp0svn.7z"
:: Create the batch file.
copy "%~dp0svn.bat" "%ROOT_DIR%"
goto :EOF


:SVN_FAIL
echo Failed to checkout svn automatically.
echo Please visit http://subversion.tigris.org to download the latest subversion client
echo before continuing.
echo You can also get the "prebacked" version used at %ROOT_URL%/third_party/
:: Still try python.
goto :C_PYTHON


:C_PYTHON
echo Installing python ...
call svn co %ROOT_URL%/third_party/python "%ROOT_DIR%\python"
if errorlevel 1 goto :PYTHON_FAIL
:: Create the batch file.
copy "%~dp0python.bat" "%ROOT_DIR%"
goto :EOF


:PYTHON_FAIL
echo Failed to checkout python automatically.
echo Please visit http://python.org to download the latest python 2.x client before
echo continuing.
echo You can also get the "prebacked" version used at %ROOT_URL%/third_party/
goto :EOF
