:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal
call "%~dp0cygwin_env.bat"
bash -c "buildbot/buildbot_toolchain_arm_untrusted.sh %*"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal
