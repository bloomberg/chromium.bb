.. _download:

Download the Native Client SDK
==============================

Follow the steps below to download and install the SDK:

1. Prerequisites:

  * Make sure you have Python 2.6 or 2.7 installed, and that the Python
    executable is in your path.

    * On Mac/Linux, Python is probably preinstalled. Run the command ``"python
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

  * On the Mac, you need to install the ``make`` command on your system before
    you can build and run the examples. One easy way to get ``make``, along
    with several other useful tools, is to install `Xcode Developer Tools
    <https://developer.apple.com/technologies/tools/>`_. After installing
    Xcode, go to the Preferences menu, select Downloads and Components, and
    verify that Command Line Tools are installed. If you'd rather not install
    Xcode, you can download and build an `open source version
    <http://mac.softpedia.com/dyn-postdownload.php?p=44632&t=4&i=1>`_ of
    ``make``.  In order to build the command you may also need to download and
    install a copy of `gcc
    <https://github.com/kennethreitz/osx-gcc-installer>`_.

2. Download the SDK update utility: `nacl_sdk.zip
   <http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/nacl_sdk.zip>`_.

3.  Unzip the SDK update utility:

  * On Mac/Linux, run the command "``unzip nacl_sdk.zip``" in a Terminal window.
  * On Windows, right-click on the .zip file and select "Extract All...". A
    dialog box will open; enter a location and click "Extract".

  Unzipping the SDK update utility creates a directory called ``nacl_sdk`` with
  the following files and directories:

  * ``naclsdk`` (and ``naclsdk.bat`` for Windows) --- the front end of the update
    utility, i.e., the command you run to download the latest bundles
  * ``sdk_cache`` --- a directory with a manifest file that lists the bundles you
    have already downloaded
  * ``sdk_tools`` --- the back end of the update utility, also known as the
    "sdk_tools" bundle

4. Go to the ``nacl_sdk`` directory and run ``naclsdk`` with the "list"
   command to see a list of available bundles. The SDK includes a separate
   bundle for each version of Chrome/Pepper (see versioning information).

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
         pepper_25 (post_stable)
         pepper_26 (post_stable)
         pepper_27 (post_stable)
         pepper_28 (stable)
         pepper_29 (beta)
         pepper_30 (dev)
         pepper_canary (canary)

  This sample output shows many bundles available for download, and that you
  have already installed the latest revision of the sdk_tools bundle (it was
  included in the zip file you downloaded). Note that the bundles are labelled
  "post-stable", "stable", "beta", "dev" and "canary". These labels correspond
  to the current versions of Chrome. In this example, Chrome 28 is stable,
  Chrome 29 is beta, etc. Therefore ``pepper_28`` is the recommended bundle to
  download, because if you released an application that used it today, it
  could be used by all current Chrome users. Note that Native Client is
  designed to be backward compatible---users of Chrome 29 can use the features
  of ``pepper_28`` and earlier.

5. Run ``naclsdk`` with the "update" command to download particular bundles that
   are available.

  On Mac/Linux::

    $ ./naclsdk update

  On Windows::

    > naclsdk update

  By default, ``naclsdk`` only downloads bundles that are recommended.  In
  general, only the "stable" bundles are recommended. Continuing with the
  earlier example, the "update" command would only download the ``pepper_28``
  bundles, since the bundles ``pepper_29`` and greater are not yet recommended.
  If you want the ``pepper_29`` bundle, you must ask for it explicitly::

    $ ./naclsdk update pepper_29

  Note that you never need update the ``sdk_tools`` bundle, it is
  updated automatically as necessary whenever ``naclsdk`` is run.

6. Run ``naclsdk`` with the "list" command again; this will show you the list of
   available bundles and verify which bundles are installed.

  On Mac/Linux::

    $ ./naclsdk list

  On Windows::

    > naclsdk list

  Continuing with the earlier example, if you previously downloaded the
  ``pepper_28`` bundle, you should see output similar to this::

    Bundles:
     I: installed
     *: update available

      I  sdk_tools (stable)
         vs_addin (dev)
         pepper_25 (post_stable)
         pepper_26 (post_stable)
         pepper_27 (post_stable)
      I  pepper_28 (stable)
         pepper_29 (beta)
         pepper_30 (dev)
         pepper_canary (canary)

7. Running ``naclsdk`` with the "update" command again will verify that your
   bundles are up-to-date, or warn if you there are new versions of previously
   installed bundles.

  On Mac/Linux::

    $ ./naclsdk update

  On Windows::

    > naclsdk update

  Continuing with the earlier example, you should see output similar to this::

    pepper_28 is already up-to-date.

  To check if there is a new version of a previously installed bundle, you can
  run the "list" command again::

    Bundles:
     I: installed
     *: update available

      I  sdk_tools (stable)
         vs_addin (dev)
         pepper_25 (post_stable)
         pepper_26 (post_stable)
         pepper_27 (post_stable)
      I* pepper_28 (stable)
         pepper_29 (beta)
         pepper_30 (dev)
         pepper_canary (canary)

  The asterisk next to the bundle name indicates that there is an update
  available. If you run  the "update" command now, ``naclsdk`` will warn you
  with a message similar to this::

    WARNING: pepper_28 already exists, but has an update available.
    Run update with the --force option to overwrite the existing directory.
    Warning: This will overwrite any modifications you have made within this directory.

  To dowload the new version of a bundle and overwrite the existing directory
  for that bundle, run ``naclsdk`` with the ``--force`` option.

  On Mac/Linux::

    $ ./naclsdk update --force

  On Windows::

    > naclsdk update --force

8.  For more information about the ``naclsdk`` utility, run:

    On Mac/Linux::

      $ ./naclsdk help

    On Windows::

      > naclsdk help

Next steps:

  * Browse through the `Release Notes
    <https://developers.google.com/native-client/sdk/release-notes>`_ for
    important information about the SDK and new bundles.
  * If you're just getting started with Native Client, we recommend reading
    the :doc:`Technical Overview <../overview>` and walking through the
    `Getting Started Tutorial
    <https://developers.google.com/native-client/devguide/tutorial>`_.
  * If you'd rather dive into information about the toolchains, see `Building
    Native Client Modules
    <https://developers.google.com/native-client/devguide/devcycle/building>`_.
