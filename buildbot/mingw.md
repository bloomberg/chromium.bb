MinGW Full Snapshot
===================

To produce our windows toolchains we use a MinGW snapshot.
This snapshot is stored in Google Storage in gs://nativeclient-private.
As we use the stock mingw-get builds of MinGW to generate the snapshot,
we've opted to not make the snapshot available for general download to avoid
the logistical burden of GPL compliance. Any current version of MinGW should
generally be adequate.
The snapshot is in practice hermetic (can be located somewhere other than
c:\MinGW).


To download and install MinGW
-----------------------------

Run the buildbot_mingw_install.py script in this directory to download the
current snapshot. This installs MinGW in native_client/MinGW.


Steps to create a new MinGW zip file
------------------------------------

1. Clobber c:\MinGW if it exists.

2. Download and install [mingw-get-setup.exe](
   http://sourceforge.net/projects/mingw/files/Installer/)

   You can uncheck the shortcut install options.

   Select these packages from Basic Setup:
     - mingw-developer-tools
     - mingw32-base
     - mingw-gcc-g++
     - msys-base

3. Put this text in c:\MinGW\msys\1.0\etc\fstab

   `c:\MinGW /mingw`

4. Download and install gsutil in your path (if you don't have it already).
   You can get it [here](http://storage.googleapis.com/pub/gsutil.tar.gz).

5. Zip c:\MinGW to a filename with a name in the format: mingw_YYYY_MM_DD.zip.
   Be sure to use the gui and save to a zipfile containing the MinGW directory.

6. Archive to gs://nativeclient-private/mingw_YYYY_MM_DD.zip using:

   `gsutil cp mingw_YYYY_MM_DD.zip gs://nativeclient-private/`
