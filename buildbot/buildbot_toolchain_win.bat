:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal
rmdir /s /q %~dp0..\tools\sdk

echo @@@BUILD_STEP install mingw@@@
call python.bat buildbot\buildbot_mingw_install.py

call "%~dp0cygwin_env.bat"
bash -c "buildbot/buildbot_toolchain.sh win"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal
