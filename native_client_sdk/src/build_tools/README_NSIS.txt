Note: This whole process takes about 1:30 to 2 hours for someone who is pretty
familiar with it.  If this is your first attempt, it's best to plan for this to
take the whole day.

Step 0: Make sure your toolchain dir is clean.
  The easiest way to do this is to remove these dirs and then do a gclient sync:
    rmdir src\toolchain
    rmdir src\third_party
    scons -c examples
    gclient sync
  Or: start with a fresh checkout from nothing.

Step 1: Generate a base copy of make_native_client_sdk.nsi
  Edit generate_windows_installer.py so that it runs everything up to
    make_native_client_sdk2.sh.
  Easiest: add a 'break' after waiting for make_native_client_sdk.sh subprocess.
    This is around line 194.
  In src, python build_tools\generate_installers.py --development
  This will produce build_tools\make_native_client_sdk.nsi

Step 3: Create a back-up and an editable copy of make_native_client_sdk.nsi
  Make 2 copies of make_native_client_sdk.nsi:
    copy make_native_client_sdk.nsi make_native_client_sdk-orig.nsi
    copy make_native_client_sdk.nsi make_native_client_sdk-new.nsi

Step 4: Edit the .nsi file to contain the appropriate changes.
  Edit the "-new" version of make_native_client_sdk.
  Manually apply the patch in nsi_patch.txt (and any other necessary edits) to
    make_native_client_sdk-new.nsi

Step 5: Create the new patch file.
  cd build_tools
  copy nsi_patch.txt nsi_patch-old.txt
  ..\third_party\cygwin\bin\diff -Naur make_native_client_sdk-orig.nsi make_native_client_sdk-new.nsi > nsi_patch.txt
  Note: the order of -orig and -new is important.

Step 6: Edit the patch file.
  Edit nsi_patch.txt
  Change every occurrence of native_client_sdk_0_x_y to %NACL_SDK_FULL_NAME%
  Change every occurrence of third_party\cygwin\ to %$CYGWIN_DIR%
    Note: the trailing '\' on third_party\cygwin_dir\ is important.
  At the top of the file, change the diff file names to both be
    make_native_client_sdk.nsi

Step 7: Test the patch.
  (In the build_tools dir):
  copy make_native_client_sdk-orig.nsi make_native_client_sdk.nsi
  ..\third_party\cygwin\bin\bash make_native_client_sdk2.sh -V 0.x.y -v -n
    Note: |x| and |y| have to match the original values in
      native_client_sdk_0_x_y from Step 6.
    Note: This step can take a long time (5 min or more).

Step 8: Test the installer.
  Step 7 should have produced nacl_sdk.exe in the root of the nacl-sdk project.
  Run this from the Finder, and install the SDK to see if it works.

Step 9: Upload the changes.
  If everything works, make a CL.
  REMEMBER: remove the 'break' statement you added in Step 1.
