.. _download:

Download the Native Client SDK
==============================

To build Native Client modules, you must download and install the Native Client
Software Development Kit (SDK). This page provides an overview of the Native
Client SDK, and instructions for how to download and install the SDK.

.. raw:: html
  
  <div id="home">
  <a class="button-nacl button-download" href="http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/nacl_sdk.zip">Download SDK Zip File</a>
  </div>

Overview
--------

The Native Client SDK includes the following:

- **Support for multiple Pepper versions** to compile for specific minimum
  versions of Chrome.
- **Update utility** to download new bundles that are available, as well as new
  versions of existing bundles.
- **Toolchains** to compile for Portable Native Client (PNaCl), traditional
  Native Client (NaCl), and for compiling architecture-specific Native Client
  applications with glibc.
- **Examples** Including C or C++ source files and header files illustrating
  how to use NaCl and Pepper, and Makefiles to build the example with each of
  the toolchains.
- **Tools** for validating Native Client modules and running modules from the
  command line.

Follow the steps below to download and install the Native Client SDK.

Prerequisites
-------------

* **Python 2.6 or 2.7:** Make sure that the Python executable is in your path.
  Python 2.7 is preferred. Python 3.x is not yet supported.
  
  * On Mac and Linux, Python is likely preinstalled. Run the command "``python
    -V``" in a terminal window, and make sure that the version you have is 2.6.x
    or 2.7.x.
  * On Windows, you may need to install Python. Go to
    `http://www.python.org/download/ <http://www.python.org/download/>`_ and
    select the latest 2.x version. In addition, be sure to add the Python
    directory (for example, ``C:\python27``) to the PATH `environment
    variable <http://en.wikipedia.org/wiki/Environment_variable>`_. Run
    "``python -V``" from a command line to verify that you properly configured
    the PATH variable.

* **Make:** On the Mac, you need to install the ``make`` command on your system
  before you can build and run the examples in the SDK. One easy way to get
  ``make``, along with several other useful tools, is to install
  `Xcode Developer Tools <https://developer.apple.com/technologies/tools/>`_.
  After installing Xcode, go to the XCode menu, open the Preferences dialog box
  then select Downloads and Components. Verify that Command Line Tools are
  installed. If you'd rather not install Xcode, you can download and build an
  `open source version
  <http://mac.softpedia.com/dyn-postdownload.php?p=44632&t=4&i=1>`_ of ``make``.
  To build the command you may also need to download and install
  `gcc <https://github.com/kennethreitz/osx-gcc-installer>`_.

Installing the SDK
------------------

#. Download the SDK update zip file: `nacl_sdk.zip
   <http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/nacl_sdk.zip>`_.

#. Unzip the file:

   * On Mac/Linux, run the command "``unzip nacl_sdk.zip``" in a terminal
     window.
   * On Windows, right-click on the .zip file and select "Extract All...". A
     dialog box will open; enter a location and click "Extract".

   A directory is created called ``nacl_sdk`` with the following files and
   directories:

   * ``naclsdk`` (and ``naclsdk.bat`` for Windows) --- the update utility,
     which is the command you run to download and update bundles.
   * ``sdk_cache`` --- a directory with a manifest file that lists the bundles
     you have already downloaded.
   * ``sdk_tools`` --- the code run by the ``naclsdk`` command.

.. installing-bundles:

Installing bundles
------------------

#. To see the SDK bundles that are available for download, go to the 
   ``nacl_sdk`` directory and run ``naclsdk`` with the "``list``" command. The
   SDK includes a separate bundle for each version of Chrome/Pepper.

   On Mac/Linux::

     $ cd nacl_sdk
     $ ./naclsdk list

   On Windows::

     > cd nacl_sdk
     > naclsdk list

   You should see output similar to this::

    Bundles:
     I: installed
     *: update available

      I  sdk_tools (stable)
         vs_addin (dev)
         pepper_31 (post_stable)
         pepper_32 (post_stable)
         pepper_33 (post_stable)
         pepper_34 (post_stable)
         pepper_35 (stable)
         pepper_36 (beta)
         pepper_37 (dev)
         pepper_canary (canary)
         bionic_canary (canary)


   The sample output above shows that several bundles are available for
   download, and that you have already installed the latest revision of the
   ``sdk_tools`` bundle. (It was included in the zip file you downloaded.) Each
   bundle is labeled post-stable, stable, beta, dev, or canary. These labels
   usually correspond to the current versions of Chrome.
   
   We recommend that you download and use a "stable" bundle, because
   applications developed with "stable" bundles can be used by all current
   Chrome users. This is because Native Client is designed to be
   backward-compatible (for example, applications developed with the
   ``pepper_31`` bundle can run in Chrome 31, Chrome 32, etc.).

#. Run ``naclsdk`` with the "update" command to download recommended bundles.

   On Mac/Linux::

     $ ./naclsdk update

   On Windows::

     > naclsdk update

   By default, ``naclsdk`` only downloads bundles that are recommended---
   generally those that are "stable." Continuing with the earlier example, the
   "update" command would only download the ``pepper_35`` bundle, since the
   bundles ``pepper_36`` and greater are not yet stable. If you want the
   ``pepper_36`` bundle, you must ask for it explicitly::

     $ ./naclsdk update pepper_36

.. Note::
  :class: note
  
   You never need to update the ``sdk_tools`` bundle. It is updated
   automatically (if necessary) whenever you run ``naclsdk``.

.. updating-bundles:

Updating bundles
------------------------------------------------------

#. Run ``naclsdk`` with the "list" command. This shows you the list of available
   bundles and verifies which bundles you have installed.

   On Mac/Linux::

     $ ./naclsdk list

   On Windows::

     > naclsdk list
     
   If an update is available, you'll see something like this.::

    Bundles:
     I: installed
     *: update available

      I  sdk_tools (stable)
         vs_addin (dev)
         pepper_31 (post_stable)
         pepper_32 (post_stable)
         pepper_33 (post_stable)
         pepper_34 (post_stable)
      I* pepper_35 (stable)
         pepper_36 (beta)
         pepper_37 (dev)
         pepper_canary (canary)
         bionic_canary (canary)

   An asterisk next to a bundle indicates that there is an update available it.
   If you run "``naclsdk update``" now, it warns you with a message similar to
   this::

     WARNING: pepper_35 already exists, but has an update available. Run update
     with the --force option to overwrite the existing directory. Warning: This
     will overwrite any modifications you have made within this directory.

#. To download and install the new bundle, run:

   On Mac/Linux::

     $ ./naclsdk update --force

   On Windows::

     > naclsdk update --force
     
Help with the ``naclsdk`` utility
---------------------------------

#. For more information about the ``naclsdk`` utility, run:

   On Mac/Linux::

     $ ./naclsdk help

   On Windows::

     > naclsdk help

**Next steps:**

* Browse through the `Release Notes <release-notes>`_ for important
  information about the SDK and new bundles.
* If you're just starting with Native Client, we recommend reading the 
  `Technical Overview <../overview>`_ and walking through the
  `Getting Started Tutorial </devguide/tutorial/tutorial-part1>`_.
* If you'd rather dive into information about the toolchains, see
  `Building Native Client Modules </devguide/devcycle/building>`_.
