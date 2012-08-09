:: Copyright (c) 2012 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal
:: Load the hermetic cygwin environment (downloading/installing/updating
:: it if necessary).
call "%~dp0cygwin_env.bat"
bash -c "buildbot/buildbot_pnacl.sh %*"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal
