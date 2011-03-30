@echo off
setlocal
set HERMETIC_CYGWIN=hermetic_cygwin_1_7_9-0_1
if exist "%~dp0..\cygwin\%HERMETIC_CYGWIN%.txt" goto :skip_cygwin_install
if not exist "%~dp0..\cygwin" goto :dont_remove_cygwin
rmdir /s /q "%~dp0..\cygwin"
if errorlevel 1 goto :rmdir_fail
:dont_remove_cygwin
cscript //nologo //e:jscript "%~dp0get_file.js" http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/cygwin_mirror/%HERMETIC_CYGWIN%.exe "%~dp0%HERMETIC_CYGWIN%.exe"
if errorlevel 1 goto :download_fail
start /WAIT %~dp0%HERMETIC_CYGWIN%.exe /DEVEL /S /D=%~dp0..\cygwin
if errorlevel 1 goto :install_fail
set CYGWIN=nodosfilewarning
"%~dp0..\cygwin\bin\touch" "%~dp0..\cygwin\%HERMETIC_CYGWIN%.txt"
if errorlevel 1 goto :install_fail
del /f /q "%~dp0%HERMETIC_CYGWIN%.exe"
:skip_cygwin_install
endlocal
set "PATH=%~dp0..\cygwin\bin;%PATH%"
goto :end
:rmdir_fail
endlocal
echo Failed to remove old version of cygwin
set ERRORLEVEL=1
goto :end
:download_fail
endlocal
echo Failed to download cygwin
set ERRORLEVEL=1
goto :end
:install_fail
endlocal
echo Failed to install cygwin
set ERRORLEVEL=1
goto :end
:end
