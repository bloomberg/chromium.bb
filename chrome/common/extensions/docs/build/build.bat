@echo off

:: Runs the win32 python interpreter checked into third_party\python_24 on
:: windows. cygwin python will not work because it will path /cygdrive/... 
:: paths to test_shell.

..\..\..\..\..\third_party\python_24\python.exe build.py