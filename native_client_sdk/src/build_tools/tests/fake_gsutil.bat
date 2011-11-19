@echo off
rem Simple Wrapper function to allow running fake_gsutil on Windows
python "%~dp0fake_gsutil.py" %*
