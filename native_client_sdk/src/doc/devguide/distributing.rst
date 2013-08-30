Distributing Your Application (NaCl)
====================================

.. {% setvar pepperversion %}pepper28{% endsetvar %}
.. {% include "native-client/_local_variables.html" %}

.. contents:: Table Of Contents
  :local:
  :backlinks: none
  :depth: 3

This document describes how to distribute Portable Native Client applications
on the web, and Native Client applications through the
`Chrome Web Store </chrome/web-store/docs/>`_ (CWS).


Introduction
------------

Portable Native Client
......................

Portable Native Client is enabled by default for web pages. The only constraint
is that the .html file, .nmf file (Portable Native Client manifest file), and
.pexe file (Portable Native Client module) must be served from the same domain.
Portable Native Client applications may also be distributed through the Chrome
Web Store.

Chrome Web Store
................

Native Client is only enabled by default for applications distributed through
the Chrome Web Store (CWS). That means that Native Client applications must be
distributed through the Chrome Web Store. The CWS requirement is in place to
prevent the proliferation of Native Client executables (.nexe files) compiled
for specific architecures (e.g., x86-32, x86-64, or ARM).

The Chrome Web Store provides a number of
`benefits
<http://www.google.com/intl/en/landing/chrome/webstore/create/why-build-apps.html>`_,
such as reaching a large number of potential customers, making it easy for
users to discover your applications, and providing a way to earn revenue from
your applications. You can upload your applications to the CWS for free, and
distribute them for free or for payment.

CWS application types
.....................

Before you upload an application to the Chrome Web Store, you must choose how
you want to distribute the application.  The CWS can be used to distribute
three types of applications: hosted applications, packaged applications, and extensions.

* A **packaged application** is an application that is bundled into one
  file, hosted in the Chrome Web Store, and downloaded to the user's machine.
* An **extension** is similar to a packaged application&mdash;the
  application is bundled into one file, hosted in the Chrome Web Store,
  and downloaded to the user's machine&mdash;but the application is
  specifically designed to extend the functionality of the Chrome browser.
* A **hosted application** is an application that is hosted somewhere
  other than the Chrome Web Store (such as your own web server or
  Google App Engine). The CWS only contains metadata that points to the
  hosted application.

The main factors to consider in choosing between these application types are:

   hosting
      You must make your own arrangements to host a hosted application (and
      pay for hosting services). Packaged applications
      and extensions are hosted in the Chrome Web Store.

   size
      Hosted applications do not have a size limit; packaged applications and
      extensions are limited to 2 GB in size.

   behavior
      Hosted applications and packaged applications are like normal web
      applications (they present their main functionality using
      an HTML page); extensions can add functionality to the Chrome browser.

For help choosing how to distribute your application, refer to
`Choosing an App Type </chrome/web-store/docs/choosing>`_.

The next section of this document provides details about how to distribute each type of application.

Distribution details
--------------------

.. _packaged:

Packaged application
....................

A packaged application is a special zip file (with a .crx extension) hosted in
the Chrome Web Store. This file contains all of the application parts: A Chrome
Web Store manifest file (manifest.json), an icon, and all of the regular Native
Client application files. Refer to
`Packaged Apps <https://developer.chrome.com/apps/about_apps.html>`_
for more information about creating a packaged application.

Once you have a CWS manifest file and an icon for your application, you can zip up all the files for your packaged application into
one zip file for upload to the Chrome Web Store. Refer to
`Step 5 </chrome/web-store/docs/get_started_simple#step5>`_
of the CWS Getting Started Tutorial to learn how to zip up your packaged
application and upload it to the Chrome Web Store.

.. _multi-platform-zip:

Reducing the size of the user download package
++++++++++++++++++++++++++++++++++++++++++++++

.. Note::
   :class: note

   **Tip:**
   Packaging an app in a multi-platform zip file can significantly reduce the
   download and storage requirements for the app.

As described above, to upload a packaged app to the Chrome Web Store (CWS) you
have to create a zip file with all the resources that your app needs, including
.nexe files for multiple architectures (x86-64, x86-32, and ARM). Prior to
Chrome 28, when users installed your app they had to download a .crx file from
the CWS with all the included .nexe files.

Starting with Chrome 28, the Chrome Web Store includes a feature called
**multi-platform zip files.**
This feature lets you structure your application directory and zip file in a
way that reduces the size of the user download package.  Here's how this
feature works:

* You still include all the .nexe files in the zip file that you upload to
  the CWS, but you designate specific .nexe files (and other files if
  appropriate) for specific architectures.
* The Chrome Web Store re-packages your app, so that users only download
  the files that they need for their specific architecture.

Here is how to use this feature:

1. Create a directory called ``_platform_specific``.
      Put this directory at the same level where your CWS manifest file,
      ``manifest.json``, is located.

2. Create a subdirectory for each specific architecture that you support,
   and add the files for each architecture in the relevant subdirectory.

      Here is a sample app directory structure:
   
      .. naclcode::
         :prettyprint: 0
   
            |-- my_app_directory/
            |       |-- manifest.json
            |       |-- my_app.html
            |       |-- my_module.nmf
            |       +-- css/
            |       +-- images/
            |       +-- scripts/
            |       |-- **_platform_specific/**
            |       |       |-- x86-64/
            |       |       |       |-- my_module_x86_64.nexe
            |       |       |-- x86-32/
            |       |       |       |-- my_module_x86_32.nexe
            |       |       |-- arm/
            |       |       |       |-- my_module_arm.nexe
            |       |       |-- all/
            |       |       |       |-- my_module_x86_64.nexe
            |       |       |       |-- my_module_x86_64.nexe
            |       |       |       |-- my_module_x86_32.nexe
   
      Please note a few important points about the app directory structure:
   
      * The architecture-specific subdirectories:
   
        * can have arbitrary names;
        * must be directly under the ``_platform_specific`` directory; and
        * must be listed in the CWS manifest file (see step 3 below).
   
      * You can include a fallback subdirectory that provides a download package
        with all the architecture-specific files.  (In the example above this
        is the ``all/`` subdirectory.) This folder is used if the user has an
        earlier version of Chrome (prior to Chrome 28) that does not support
        multi-platform zip files.

      * You cannot include any files directly in the folder
        ``_platform_specific``.  All architecture-specific files
        must be under one of the architecture-specific subdirectories.

      * Files that are not under the ``_platform_specific`` directory are
        included in all download packages.  (In the example above, that
        includes ``my_app.html``, ``my_module.nmf``,
        and the ``css/``, ``images/``, and ``scripts/`` directories.)


3. Modify the CWS manifest file, ``manifest.json``, so that it specifies which
   subdirectory under ``_platform_specific`` corresponds to which architecture.

      The CWS manifest file must include a new name/value pair, where the name
      is ``platforms`` and the value is an array.  The array has an object for
      each Native Client architecture with two name/value pairs:
   
      +----------------------+---------------------------------------+
      | Name                 | Value                                 |
      +======================+=======================================+
      | ``nacl_arch``        | ``x86-64``, ``x86-32``, or ``arm``    |
      +----------------------+---------------------------------------+
      | ``sub_package_path`` | the path of the directory (starting   |
      |                      | with ``_platform_specific``) that     |
      |                      | contains the files for the designated |
      |                      | NaCl architecture                     |
      +----------------------+---------------------------------------+
   
      Here is a sample ``manifest.json`` file:
   
      .. naclcode::
         :prettyprint: 0
   
         {
           "name": "My Reminder App",
           "description": "A reminder app that syncs across Chrome browsers.",
           "manifest_version": 2,
           "minimum_chrome_version": "28",
           "offline_enabled": true,
           "version": "0.3",
           "permissions": [
             {"fileSystem": ["write"]},
             "alarms",
             "storage"
           ],
           "app": {
             "background": {
               "scripts": ["scripts/background.js"]
             }
           },
           "icons": {
             "16": "images/icon-16x16.png",
             "128": "images/icon-128x128.png"
           },
           **"platforms": [
             {
               "nacl_arch": "x86-64",
               "sub_package_path": "_platform_specific/x86-64/"
             },
             {
               "nacl_arch": "x86-32",
               "sub_package_path": "_platform_specific/x86-32/"
             },
             {
               "nacl_arch": "arm",
               "sub_package_path": "_platform_specific/arm/"
             },
             {
               "sub_package_path": "_platform_specific/all/"
             }
           ]**
         }
   
      Note the last entry in the CWS manifest file above, which specifies a
      ``sub_package_path`` without a corresponding ``nacl_arch``. This entry
      identifies the fallback directory, which is included in the download
      package if the user architecture does not match any of the listed NaCl
      architectures, or if the user is using an older version of Chrome that
      does not support multi-platform zip files.

4. Modify your application as necessary so that it uses the files for the
   correct user architecture.

      To reference architecture-specific files, use the JavaScript API
      `chrome.runtime.getPlatformInfo() <http://developer.chrome.com/trunk/extensions/runtime.html#method-getPlatformInfo>`_.
      As an example, if you have architecture-specific files in the directories
      ``x86-64``, ``x86-32``, and ``arm``, you can use the following JavaScript
      code to create a path for the files:
   
      .. naclcode::
   
         function getPath(name) {
           return '_platform_specific/' +
             chrome.runtime.getPlatformInfo().nacl_arch +
             '/' + name;
         }

5. Test your app, create a zip file, and upload the app to the CWS as before.

Additional considerations for a packaged application
++++++++++++++++++++++++++++++++++++++++++++++++++++

* In the description of your application in the CWS, make sure to mention that
  your application is a Native Client application that only works with the
  Chrome browser. Also make sure to identify the minimum version of Chrome
  that your application requires.
* Hosted and packaged applications have a "launch" parameter in the CWS
  manifest. This parameter is present only in apps (not extensions), and it
  tells Google Chrome what to show when a user starts an installed app. For
  example:

  .. naclcode::

     "launch": {
       "web_url": "http://mail.google.com/mail/"
     }

* If you want to write local data using the Pepper
  `FileIO </native-client/peppercpp/classpp_1_1_file_i_o>`_
  API, you must set the 'unlimitedStorage' permission in your Chrome Web
  Store manifest file, just as you would for a JavaScript application that
  uses the HTML5 File API.
* For packaged applications, you can only use in-app purchases.
* You can place your application in the Google Web Store with access only to
  certain people for testing. See
  `Publishing to test accounts </chrome/web-store/docs/publish#testaccounts>`_
  for more information.

Extension
.........

An extension consists of a special zip file (with a .crx extension) hosted in
the Chrome Web Store containing all of the application parts: A Chrome Web
Store manifest file (manifest.json), an icon, and all of the regular Native
Client application files. Refer to the
`Google Chrome Extensions Overview <http://code.google.com/chrome/extensions/overview.html>`_
to learn how to create an extension.

Once you have a CWS manifest file and an icon for your application, you can zip
up all the files for your packaged application into one zip file for upload to
the Chrome Web Store. Refer to
`Step 5 </chrome/web-store/docs/get_started_simple#step5>`_
of the CWS Getting Started Tutorial to learn how to zip up your extension and
upload it to the Chrome Web Store.

.. Note::
   :class: note

   **Tip:** Use a :ref:`multi-platform zip file <multi-platform-zip>` to reduce
   the download and storage requirements for your extension.

Hosted application
..................

A hosted application is a normal web application with some extra metadata for
the Chrome Web Store. Specifically, a hosted application consists of three
parts:

1. A Chrome Web Store manifest file (manifest.json) and an icon uploaded to the
   Chrome Web Store. The manifest file points to the Native Client application,
   essentially a web site, hosted on your web server. You must put the CWS
   manifest file and the icon into a zip file for upload to the Chrome Web
   store, and then convert that file to a special zip file with a .crx
   extension. Refer to the Chrome Web Store
   `Tutorial: Getting Started </chrome/web-store/docs/get_started_simple>`_
   or to the
   `Hosted Apps </chrome/apps/docs/developers_guide>`_
   documentation for information about how to create and upload this metadata
   to the CWS.

2. All the regular Native Client application files. These files are hosted on
   your web server or Google App Engine at a location pointed to by the CWS
   manifest file.

3. Binary assets. These files are hosted on your web server, Google App Engine,
   or Data Storage for Developers.

Hosting options
+++++++++++++++

Google offers a couple of hosting options you might consider instead of your
own:

* Google App Engine allows developers to release their applications on the same
  platform that Google uses for applications such Gmail.  Refer to
  `Google App Engine - Pricing and Features <http://www.google.com/enterprise/cloud/appengine/pricing.html>`_
  for pricing information.

* Google Storage for Developers allows developers to store application data
  using Google's cloud services and is available on a fee-per-usage basis.
  Refer to `Pricing and Support </storage/docs/pricingandterms>`_
  for pricing information.

Additional considerations for a hosted application
++++++++++++++++++++++++++++++++++++++++++++++++++

* The .html file, .nmf file (Native Client manifest file), and .nexe files
  (compiled Native Client modules) must be served from the same domain, and the
  Chrome Web Store manifest file must specify the correct, verified domain.
  Other files can be served from the same or another domain.

* In the description of your application in the CWS, make sure to mention that
  your application is a Native Client application that only works with the
  Chrome browser. Also make sure to identify the version of Chrome that your
  application requires.

* Hosted and packaged applications have a "launch" parameter in the CWS
  manifest. This parameter is present only in apps (not extensions), and it
  tells Google Chrome what to show when a user starts an installed app. For
  example:

  .. naclcode::
     :prettyprint: 0

     "launch": {
       "web_url": "http://mail.google.com/mail/"
     }

* If you want to write local data using the Pepper
  `FileIO </native-client/peppercpp/classpp_1_1_file_i_o>`_
  API, you must set the 'unlimitedStorage' permission in your Chrome Web
  Store manifest file, just as you would for a JavaScript application that uses
  the HTML5 File API.

* You can place your application in the Google Web Store with access only to
  certain people for testing.  See
  `Publishing to test accounts </chrome/web-store/docs/publish#testaccounts>`_
  for more information.

Additional considerations
=========================

Registering Native Client modules to handle MIME types
------------------------------------------------------

If you want Chrome to use a Native Client module to display a particular type
of content, you can associate the MIME type of that content with the Native
Client module. Use the ``nacl_modules`` attribute in the Chrome Web Store
manifest file to register a Native Client module as the handler for one or more
specific MIME types. For example, the bold code in the snippet below registers
a Native Client module as the content handler for the OpenOffice spreadsheet
MIME type:

.. naclcode::
   :prettyprint: 0

   {
      "name": "My Native Client Spreadsheet Viewer",
      "version": "0.1",
      "description": "Open spreadsheets right in your browser.",
      **"nacl_modules": [{
         "path": "SpreadsheetViewer.nmf",
         "mime_type": "application/vnd.oasis.opendocument.spreadsheet"
      }]**
   }

The value of "path" is the location of a Native Client manifest file (.nmf)
within the application directory. For more information on Native Client
manifest files, see
`Files in a Native Client application </native-client/overview#application-files>`_.

The value of "mime_type" is a specific MIME type that you want the Native
Client module to handle. Each MIME type can be associated with only one .nmf
file, but a single .nmf file might handle multiple MIME types. The following
example shows an extension with two .nmf files that handle three MIME types.

.. naclcode::
   :prettyprint: 0

   {
      "name": "My Native Client Spreadsheet and Document Viewer",
      "version": "0.1",
      "description": "Open spreadsheets and documents right in your browser.",
      "nacl_modules": [{
        "path": "SpreadsheetViewer.nmf",
        "mime_type": "application/vnd.oasis.opendocument.spreadsheet"
      },
      {
         "path": "SpreadsheetViewer.nmf",
         "mime_type": "application/vnd.oasis.opendocument.spreadsheet-template"
      },
      {
         "path": "DocumentViewer.nmf",
         "mime_type": "application/vnd.oasis.opendocument.text"
      }]
   }

The ``nacl_modules`` attribute is optional&mdash;specify this attribute only if
you want Chrome to use a Native Client module to display a particular type of
content.

Using CWS inline install
------------------------

Once you've published an application, you may be wondering how users will find
and install the application. For users who browse the Chrome Web Store and find
your application, installing the application is a simple one-click process.
However, if a user is already on your site, it can be cumbersome for them to
complete the installation&mdash;they would need to navigate away from your site
to the CWS, complete the installation process, and then return to your site. To
address this issue, you can initiate installation of applications "inline" from
your site&mdash;the applications are still hosted in the Chrome Web Store, but
users no longer have to leave your site to install them. See
`Using Inline Installation </chrome/web-store/docs/inline_installation>`_
for information on how to use this feature.

Monetizing applications and extensions
--------------------------------------

Google provides three primary monetization options for Native Client
applications: in-app payments, one-time charges, and subscriptions.  Refer to
`Monetizing Your App </chrome/web-store/docs/money>`_
to learn about these options. The
`Chrome Web Store Overview </chrome/web-store/docs/>`_
also has information on different approaches to charging for your application.
