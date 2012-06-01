:: Copyright (c) 2012 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal
set "PATH=c:\cygwin\bin;%PATH%"
bash -c "buildbot/buildbot_pnacl.sh %*"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal
