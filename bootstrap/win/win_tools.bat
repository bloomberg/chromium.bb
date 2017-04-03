@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This script will determine if python, git, or svn binaries need updates.  It
:: returns 123 if the user's shell must restart, otherwise !0 is failure

:: Sadly, we can't use SETLOCAL here otherwise it ERRORLEVEL is not correctly
:: returned.

set CHROME_INFRA_URL=https://storage.googleapis.com/chrome-infra/
:: It used to be %~dp0 but ADODB.Stream may fail to write to this directory if
:: the directory DACL is set to elevated integrity level.
set ZIP_DIR=%TEMP%

:: Get absolute root directory (.js scripts don't handle relative paths well).
pushd %~dp0..\..
set WIN_TOOLS_ROOT_DIR=%CD%
popd

if "%1" == "force" (
  set WIN_TOOLS_FORCE=1
  shift /1
)


:PYTHON_CHECK
if not exist "%WIN_TOOLS_ROOT_DIR%\python276_bin" goto :PY27_INSTALL
if not exist "%WIN_TOOLS_ROOT_DIR%\python.bat" goto :PY27_INSTALL
set ERRORLEVEL=0
goto :GIT_CHECK


:PY27_INSTALL
echo Installing python 2.7.6...
:: Cleanup python directory if it was existing.
set PYTHON_URL=%CHROME_INFRA_URL%python276_bin.zip
if exist "%WIN_TOOLS_ROOT_DIR%\python276_bin\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\python276_bin"
if exist "%ZIP_DIR%\python276.zip" del "%ZIP_DIR%\python276.zip"
echo Fetching from %PYTHON_URL%
cscript //nologo //e:jscript "%~dp0get_file.js" %PYTHON_URL% "%ZIP_DIR%\python276_bin.zip"
if errorlevel 1 goto :PYTHON_FAIL
:: Will create python276_bin\...
cscript //nologo //e:jscript "%~dp0unzip.js" "%ZIP_DIR%\python276_bin.zip" "%WIN_TOOLS_ROOT_DIR%"
:: Create the batch files.
call copy /y "%~dp0python276.new.bat" "%WIN_TOOLS_ROOT_DIR%\python.bat" 1>nul
call copy /y "%~dp0pylint.new.bat" "%WIN_TOOLS_ROOT_DIR%\pylint.bat" 1>nul
del "%ZIP_DIR%\python276_bin.zip"
set ERRORLEVEL=0
goto :GIT_CHECK


:PYTHON_FAIL
echo ... Failed to checkout python automatically.
echo You should get the "prebaked" version at %PYTHON_URL%
set ERRORLEVEL=1
goto :END

:GIT_CHECK
"%WIN_TOOLS_ROOT_DIR%\python.bat" "%~dp0git_bootstrap.py"
goto :END

:returncode
set WIN_TOOLS_ROOT_DIR=
exit /b %ERRORLEVEL%

:END
call :returncode %ERRORLEVEL%
