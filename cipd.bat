@echo off
:: Copyright (c) 2016 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal

set CIPD_CLIENT_SRV=https://chrome-infra-packages.appspot.com
for /f %%i in (%~dp0cipd_client_version) do set CIPD_CLIENT_VER=%%i

if not exist "%~dp0.cipd_client.exe" (
  call :CLEAN_BOOTSTRAP
  goto :EXEC_CIPD
)

call :SELF_UPDATE
if %ERRORLEVEL% neq 0 (
  echo CIPD client self-update failed, trying to bootstrap it from scratch... 1>&2
  call :CLEAN_BOOTSTRAP
)

:EXEC_CIPD

if %ERRORLEVEL% neq 0 (
  echo Failed to bootstrap or update CIPD client 1>&2
)
if %ERRORLEVEL% equ 0 (
  "%~dp0.cipd_client.exe" %*
)

endlocal & (
  set EXPORT_ERRORLEVEL=%ERRORLEVEL%
)
exit /b %EXPORT_ERRORLEVEL%


:: Functions below.
::
:: See http://steve-jansen.github.io/guides/windows-batch-scripting/part-7-functions.html
:: if you are unfamiliar with this madness.


:CLEAN_BOOTSTRAP
:: To allow this powershell script to run if it was a byproduct of downloading
:: and unzipping the depot_tools.zip distribution, we clear the Zone.Identifier
:: alternate data stream. This is equivalent to clicking the "Unblock" button
:: in the file's properties dialog.
echo.>%~dp0cipd.ps1:Zone.Identifier
powershell -NoProfile -ExecutionPolicy RemoteSigned -Command "%~dp0cipd.ps1" < nul
if %ERRORLEVEL% equ 0 (
  :: Need to call SELF_UPDATE to setup .cipd_version file.
  call :SELF_UPDATE
)
exit /B %ERRORLEVEL%


:SELF_UPDATE
"%~dp0.cipd_client.exe" selfupdate -version "%CIPD_CLIENT_VER%" -service-url "%CIPD_CLIENT_SRV%"
exit /B %ERRORLEVEL%
