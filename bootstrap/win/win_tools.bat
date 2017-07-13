@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This script will determine if python or git binaries need updates. It
:: returns !0 as failure

:: Note: we set EnableDelayedExpansion so we can perform string manipulations
:: in our manifest parsing loop. This only works on Windows XP+.
setlocal EnableDelayedExpansion

set CHROME_INFRA_URL=https://storage.googleapis.com/chrome-infra/
:: It used to be %~dp0 but ADODB.Stream may fail to write to this directory if
:: the directory DACL is set to elevated integrity level.
set ZIP_DIR=%TEMP%

:: Get absolute root directory (.js scripts don't handle relative paths well).
pushd %~dp0..\..
set WIN_TOOLS_ROOT_DIR=%CD%
popd

:: Extra arguments to pass to our "win_tools.py" script.
set WIN_TOOLS_EXTRA_ARGS=
set WIN_TOOLS_PYTHON_BIN=%WIN_TOOLS_ROOT_DIR%\python.bat

:: TODO: Deprecate this when legacy mode is disabled.
if "%1" == "force" (
  set WIN_TOOLS_EXTRA_ARGS=%WIN_TOOLS_EXTRA_ARGS% --force
  shift /1
)

:: Determine if we're running a bleeding-edge installation.
if not exist "%WIN_TOOLS_ROOT_DIR%\.bleeding_edge" (
  set CIPD_MANIFEST=
) else (
  set CIPD_MANIFEST=manifest_bleeding_edge.txt
  set WIN_TOOLS_EXTRA_ARGS=%WIN_TOOLS_EXTRA_ARGS% --bleeding-edge
)

:: Identify our CIPD executable. If the client executable exists, use it
:: directly; otherwise, use "cipd.bat" to bootstrap the client. This
:: optimization is useful because this script can be run frequently, and
:: reduces execution time noticeably.
::
:: See "//cipd.bat" and "//cipd.ps1" for more information.
set CIPD_EXE=%WIN_TOOLS_ROOT_DIR%\cipd.bat
set WIN_TOOLS_EXTRA_ARGS=%WIN_TOOLS_EXTRA_ARGS% --cipd-client "%CIPD_EXE%"

:: TODO: This logic will change when we deprecate legacy mode. For now, we
:: assume !bleeding_edge == legacy.
if "%CIPD_MANIFEST%" == "" goto :PY27_LEGACY_CHECK

:: We are committed to CIPD, and will use "win_tools.py" to perform our
:: finalization.
::
:: Parse our CIPD manifest and identify the "cpython" version. We do this by
:: reading it line-by-line, identifying the line containing "cpython", and
:: stripping all text preceding "version:". This leaves us with the version
:: string.
::
:: This method requires EnableDelayedExpansion, and extracts the Python version
:: from our CIPD manifest. Variables referenced using "!" instead of "%" are
:: delayed expansion variables.
for /F "tokens=*" %%A in (%~dp0%CIPD_MANIFEST%) do (
  set LINE=%%A
  if not "x!LINE:cpython=!" == "x!LINE!" set PYTHON_VERSION=!LINE:*version:=!
)
if "%PYTHON_VERSION%" == "" (
  @echo Could not extract Python version from manifest.
  set ERRORLEVEL=1
  goto :END
)

:: We will take the version string, replace "." with "_", and surround it with
:: "win-tools-<PYTHON_VERSION>_bin" so that it matches "win_tools.py"'s cleanup
:: expression and ".gitignore".
::
:: We incorporate PYTHON_VERSION into the "win_tools" directory name so that
:: new installations don't interfere with long-running Python processes if
:: Python is upgraded.
set WIN_TOOLS_NAME=win_tools-%PYTHON_VERSION:.=_%_bin
set WIN_TOOLS_PATH=%WIN_TOOLS_ROOT_DIR%\%WIN_TOOLS_NAME%
set WIN_TOOLS_EXTRA_ARGS=%WIN_TOOLS_EXTRA_ARGS% --win-tools-name "%WIN_TOOLS_NAME%"

:: Install our CIPD packages.
call "%CIPD_EXE%" ensure -ensure-file "%~dp0%CIPD_MANIFEST%" -root "%WIN_TOOLS_PATH%"
if errorlevel 1 goto :END

set WIN_TOOLS_PYTHON_BIN=%WIN_TOOLS_PATH%\python\bin\python.exe
goto :WIN_TOOLS_PY


:: LEGACY Support
::
:: This is a full Python installer. It falls through to "win_tools.py",
:: instructing it to not handle Python installation. This should be removed
:: once we commit to CIPD.
:PY27_LEGACY_CHECK
if not exist "%WIN_TOOLS_ROOT_DIR%\python.bat" goto :PY27_LEGACY_INSTALL
if not exist "%WIN_TOOLS_ROOT_DIR%\python276_bin" goto :PY27_LEGACY_INSTALL
goto :WIN_TOOLS_PY


:PY27_LEGACY_INSTALL
echo Installing python 2.7.6...
:: Cleanup python directory if it was existing.
set PYTHON_URL=%CHROME_INFRA_URL%python276_bin.zip
if exist "%WIN_TOOLS_ROOT_DIR%\python276_bin\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\python276_bin"
if exist "%ZIP_DIR%\python276.zip" del "%ZIP_DIR%\python276.zip"
echo Fetching from %PYTHON_URL%
cscript //nologo //e:jscript "%~dp0get_file.js" %PYTHON_URL% "%ZIP_DIR%\python276_bin.zip"
if errorlevel 1 goto :PYTHON_LEGACY_FAIL
:: Will create python276_bin\...
cscript //nologo //e:jscript "%~dp0unzip.js" "%ZIP_DIR%\python276_bin.zip" "%WIN_TOOLS_ROOT_DIR%"
:: Create the batch files.
call copy /y "%~dp0python276.new.bat" "%WIN_TOOLS_ROOT_DIR%\python.bat" 1>nul
call copy /y "%~dp0pylint.new.bat" "%WIN_TOOLS_ROOT_DIR%\pylint.bat" 1>nul
del "%ZIP_DIR%\python276_bin.zip"
set ERRORLEVEL=0
goto :WIN_TOOLS_PY


:PYTHON_LEGACY_FAIL
echo ... Failed to checkout python automatically.
echo You should get the "prebaked" version at %PYTHON_URL%
set ERRORLEVEL=1
goto :END


:: This executes "win_tools.py" using the WIN_TOOLS_PYTHON_BIN Python
:: interpreter.
:WIN_TOOLS_PY
call "%WIN_TOOLS_PYTHON_BIN%" "%~dp0win_tools.py" %WIN_TOOLS_EXTRA_ARGS%


:END
set EXPORT_ERRORLEVEL=%ERRORLEVEL%
endlocal & (
  set ERRORLEVEL=%EXPORT_ERRORLEVEL%
)
exit /b %ERRORLEVEL%
