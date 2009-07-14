Building and running demos via SCons and run.py
---------------------------------------------------------------

  From your native_client directory: if you haven't done
  so yet, invoke SCons to build the Native Client project.

  From the native client directory:

    To clean:
      linux, mac:
        native_client$ ./scons MODE=all -c
      cygwin:
        native_client$ ./scons.bat MODE=all -c
      windows command line:
        native_client> scons.bat MODE=all -c

    To build:
      linux, mac:
        native_client$ ./scons MODE=all
      cygwin:
        native_client$ ./scons.bat MODE=all
      windows command line:
        native_client> scons.bat MODE=all

To run a demo, there is a short python script run.py in the
demo folder.

  From the demo directory (earth used in this example):

    To run, execute the python script:
      native_client/tests/earth$ python run.py


Building and running demos from GNU Makefiles
------------------------------------------------------------
Alternatively, it is possible to build and run some demos via a GNU makefile,
if you prefer.  This will require you to have GNU make installed and
available in your path.  First, if you haven't already done so, use SCons to
build the Native Client project.  The demos with makefiles can then be locally
built & run via GNU make from within each of their directories.  See the
makefile for more details:
   native_client/tests/common/Makefile.mk
