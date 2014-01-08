.. _download:

Download the Native Client SDK
==============================

To build Native Client modules, you must download and install the Native
Client Software Development Kit (SDK). This page provides an overview
of the Native Client SDK, and instructions for how to download and
install the SDK.

Overview
--------

The Native Client SDK includes the following:

support for multiple Pepper versions
  The SDK contains **bundles** that let you compile Native Client modules
  using different versions of the
  :ref:`link_pepper` (e.g., Pepper 31 or Pepper Canary). Review the
  :doc:`Release Notes <release-notes>` for a description of the new features
  included in each Pepper version to help you decide which bundle to
  use to develop your application. In general, Native Client modules
  compiled using a particular Pepper version will work in
  corresponding versions of Chrome and higher. For example, a module
  compiled using the Pepper 31 bundle will work in Chrome 31 and
  higher.

update utility
  The ``naclsdk`` utility (``naclsdk.bat`` on Windows) lets you download new
  bundles that are available, as well as new versions of existing bundles.

toolchains
  Each platform includes three toolchains: one for compiling
  Portable Native Client (PNaCl) applications, one for compiling
  architecture-specific Native Client applications with newlib, and
  one for compiling architecture-specific Native Client applications with glibc.
  Newlib and glibc are two different implementations
  of the C standard library. All three toolchains contain
  Native Client-compatible versions of standard compilers, linkers,
  and other tools. See :doc:`NaCl and PNaCl </nacl-and-pnacl>` to help
  you choose the right toolchain.

examples
  Each example in the SDK includes C or C++ source files and header files
  illustrating how to use NaCl and Pepper, along with a Makefile to build
  the example using each of the toolchains.

tools
  The SDK includes a number of additional tools that you can use for
  tasks such as validating Native Client modules and running modules
  from the command line.

Follow the steps below to download and install the Native Client SDK.

Prerequisites
-------------

* **Python:** Make sure you have Python 2.6 or 2.7 installed, and that the
  Python executable is in your path.

  * On Mac/Linux, Python is likely preinstalled. Run the command ``"python
    -V``" in a terminal window, and make sure that the version of Python you
    have is 2.6.x or 2.7.x (if it's not, upgrade to one of those versions).
  * On Windows, you may need to install Python. Go to
    `http://www.python.org/download/ <http://www.python.org/download/>`_ and
    select the latest 2.x version. In addition, be sure to add the Python
    directory (for example, ``C:\python27``) to the PATH `environment
    variable <http://en.wikipedia.org/wiki/Environment_variable>`_. After
    you've installed Python, run the command ``"python -V``" in a Command
    Prompt window and verify that the version of Python you have is 2.6.x or
    2.7.x.
  * Note that Python 3.x is not yet supported.

* **Make:** On the Mac, you need to install the ``make`` command on your system
  before you can build and run the examples in the SDK. One easy way to get
  ``make``, along with several other useful tools, is to install
  `Xcode Developer Tools <https://developer.apple.com/technologies/tools/>`_.
  After installing Xcode, go to the Preferences menu, select
  Downloads and Components, and verify that Command Line Tools are installed.
  If you'd rather not install Xcode, you can download and build an
  `open source version
  <http://mac.softpedia.com/dyn-postdownload.php?p=44632&t=4&i=1>`_ of
  ``make``. In order to build the command you may also need to download and
  install a copy of `gcc <https://github.com/kennethreitz/osx-gcc-installer>`_.

Download and install the SDK
----------------------------

#. Download the SDK update utility: `nacl_sdk.zip
   <http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/nacl_sdk.zip>`_.

#. Unzip the SDK update utility:

   * On Mac/Linux, run the command "``unzip nacl_sdk.zip``" in a terminal
     window.
   * On Windows, right-click on the .zip file and select "Extract All...". A
     dialog box will open; enter a location and click "Extract".

   Unzipping the SDK update utility creates a directory called ``nacl_sdk`` with
   the following files and directories:

   * ``naclsdk`` (and ``naclsdk.bat`` for Windows) --- the front end of the update
     utility, i.e., the command you run to download the latest bundles
   * ``sdk_cache`` --- a directory with a manifest file that lists the bundles
     you have already downloaded
   * ``sdk_tools`` --- the back end of the update utility, also known as the
     "sdk_tools" bundle

#. To see the SDK bundles that are available for download, go to the ``nacl_sdk``
   directory and run ``naclsdk`` with the ``"list"`` command.
   The SDK includes a separate bundle for each version of Chrome/Pepper.

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
         pepper_27 (post_stable)
         pepper_28 (post_stable)
         pepper_29 (post_stable)
         pepper_30 (post_stable)
         pepper_31 (stable)
         pepper_32 (beta)
         pepper_canary (canary)

   The sample output above shows that there are a number of bundles available
   for download, and that you have already installed the latest revision of the
   ``sdk_tools`` bundle (it was included in the zip file you downloaded).
   Each bundle is labeled post-stable, stable, beta, dev, or canary.
   These labels usually correspond to the current versions of
   Chrome. (In the example above, Chrome 31 is stable, Chrome 32 is beta, etc.).
   We generally recommend that you download and use a "stable" bundle,
   as applications developed with "stable" bundles can be used by all current
   Chrome users. This is because Native Client is designed to be
   backward-compatible (for example, applications developed with the
   ``pepper_31`` bundle can run in Chrome 31, Chrome 32, etc.).
   Thus in the example above, ``pepper_31`` is the recommended bundle to use.

#. Run ``naclsdk`` with the "update" command to download recommended bundles.

   On Mac/Linux::

     $ ./naclsdk update

   On Windows::

     > naclsdk update

   By default, ``naclsdk`` only downloads bundles that are
   recommended---generally those that are "stable." Continuing with the earlier example, the
   "update" command would only download the ``pepper_31``
   bundle, since the bundles ``pepper_32`` and greater are not yet stable.
   If you want the ``pepper_32`` bundle, you must ask for it explicitly::

     $ ./naclsdk update pepper_32

   Note that you never need to update the ``sdk_tools`` bundle---it is
   updated automatically (if necessary) whenever you run ``naclsdk``.

.. Note::
  :class: note

  The minimum SDK bundle that supports PNaCl is ``pepper_31``.

Staying up-to-date and getting new versions of bundles
------------------------------------------------------

#. Run ``naclsdk`` with the "list" command again; this will show you the list of
   available bundles and verify which bundles you have installed.

   On Mac/Linux::

     $ ./naclsdk list

   On Windows::

     > naclsdk list

   Continuing with the earlier example, if you previously downloaded the
   ``pepper_31`` bundle, you should see output similar to this::

    Bundles:
     I: installed
     *: update available

      I  sdk_tools (stable)
         vs_addin (dev)
         pepper_27 (post_stable)
         pepper_28 (post_stable)
         pepper_29 (post_stable)
         pepper_30 (post_stable)
      I  pepper_31 (stable)
         pepper_32 (beta)
         pepper_canary (canary)

#. Running ``naclsdk`` with the "update" command again will verify that your
   bundles are up-to-date, or warn if you there are new versions of previously
   installed bundles.

   On Mac/Linux::

     $ ./naclsdk update

   On Windows::

     > naclsdk update

   Continuing with the earlier example, you should see output similar to this::

     pepper_31 is already up-to-date.

#. To check if there is a new version of a previously installed bundle, you can
   run the "list" command again::

    Bundles:
     I: installed
     *: update available

      I  sdk_tools (stable)
         vs_addin (dev)
         pepper_27 (post_stable)
         pepper_28 (post_stable)
         pepper_29 (post_stable)
         pepper_30 (post_stable)
      I* pepper_31 (stable)
         pepper_32 (beta)
         pepper_canary (canary)

   An asterisk next to a bundle indicates that there is an update
   available for that bundle. If you run  the "update" command now,
   ``naclsdk`` will warn you with a message similar to this::

     WARNING: pepper_31 already exists, but has an update available.
     Run update with the --force option to overwrite the existing directory.
     Warning: This will overwrite any modifications you have made within this directory.

   To dowload the new version of a bundle and overwrite the existing directory
   for that bundle, run ``naclsdk`` with the ``--force`` option.

   On Mac/Linux::

     $ ./naclsdk update --force

   On Windows::

     > naclsdk update --force

#. For more information about the ``naclsdk`` utility, run:

   On Mac/Linux::

     $ ./naclsdk help

   On Windows::

     > naclsdk help

Next steps:

* Browse through the :doc:`Release Notes <release-notes>` for important
  information about the SDK and new bundles.
* If you're just getting started with Native Client, we recommend reading
  the :doc:`Technical Overview <../overview>` and walking through the
  :doc:`Getting Started Tutorial </devguide/tutorial/tutorial-part1>`.
* If you'd rather dive into information about the toolchains, see
  :doc:`Building Native Client Modules </devguide/devcycle/building>`.
