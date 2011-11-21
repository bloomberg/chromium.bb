Section "!Native Client SDK" NativeClientSDK
  SectionIn RO
  SetOutPath $INSTDIR
  CreateDirectory "$INSTDIR\test_dir"
  File "/oname=test_file.txt" "build_tools\tests\nsis_test_archive\test_file.txt"
  File "/oname=test_dir\test_dir_file1.txt" "build_tools\tests\nsis_test_archive\test_dir\test_dir_file1.txt"
  File "/oname=test_dir\test_dir_file2.txt" "build_tools\tests\nsis_test_archive\test_dir\test_dir_file2.txt"
SectionEnd
