@echo off
setlocal
set PATH=%~dp0svn;%PATH%
"%~dp0svn\svn.exe" %*
