# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Django settings for cq_stats project

TODO(pprabhu): These settings should be instead stored in the app local DB that
AE provides.
It's probably safer that way (no settings in source), and it's easier to manage
the instance from the AE admin interface than having to update source and
relaunch.
"""

from __future__ import print_function

import os

import developer_settings as ds


DEBUG = False or ds.FORCE_DEBUG
TEMPLATE_DEBUG = DEBUG

# CIDB instance to use.
# Only used when the app is running on appengine.
# cidb test instance
# CIDB_PROJECT_NAME = 'true-oasis-601'
# CIDB_INSTANCE_NAME = 'cidb'
# cidb debug instance
CIDB_PROJECT_NAME = 'cosmic-strategy-646'
CIDB_INSTANCE_NAME = 'debug-cidb'

BASE_DIR = os.path.dirname(os.path.dirname(__file__))
PROJECT_NAME = os.path.basename(os.path.dirname(__file__))
PROJECT_DIR = os.path.join(BASE_DIR, PROJECT_NAME)

CIDB_HOST_PATH = os.path.join(ds.CIDB_CREDS_DIR, 'host.txt')
CIDB_SERVER_CA_PATH = os.path.join(ds.CIDB_CREDS_DIR, 'server-ca.pem')
CIDB_CLIENT_CERT_PATH = os.path.join(ds.CIDB_CREDS_DIR, 'client-cert.pem')
CIDB_CLIENT_KEY_PATH = os.path.join(ds.CIDB_CREDS_DIR, 'client-key.pem')
ANNOTATOR_PASSWORD_PATH = os.path.join(ds.CIDB_CREDS_DIR, 'password.txt')

cidb_host = ''
if os.path.exists(CIDB_HOST_PATH):
  with open(CIDB_HOST_PATH, 'r') as f:
    cidb_host = f.read().strip()

annotator_password = ''
if os.path.exists(ANNOTATOR_PASSWORD_PATH):
  with open(ANNOTATOR_PASSWORD_PATH, 'r') as f:
    annotator_password = f.read().strip()
if not annotator_password:
  raise Exception('Can\'t work without annotator password')

if os.getenv('SERVER_SOFTWARE', '').startswith('Google App Engine'):
  # Running on production AppEngine. Use CloudSQL via unix socket.
  default_database = {
      'ENGINE': 'django.db.backends.mysql',
      'HOST': '/cloudsql/%s:%s' % (CIDB_PROJECT_NAME,
                                   CIDB_INSTANCE_NAME),
      'NAME': 'cidb',
      'USER': 'annotator',
      'PASSWORD': annotator_password}
elif ds.SETTINGS_MODE == 'cidb_instance':
  if not cidb_host or not annotator_password:
    raise Exception('"cidb_instance" mode needs both the cidb host IP and '
                    'the password for the "annotator" user')
  default_database = {
      'ENGINE': 'django.db.backends.mysql',
      'HOST': cidb_host,
      'PORT': '3306',
      'NAME': 'cidb',
      'USER': 'annotator',
      'PASSWORD': annotator_password,
      'OPTIONS': {
          'ssl': {'ca': CIDB_SERVER_CA_PATH,
                  'cert': CIDB_CLIENT_CERT_PATH,
                  'key': CIDB_CLIENT_KEY_PATH}}}
elif ds.SETTINGS_MODE == 'local_mysql':
  default_database = {
      'ENGINE': 'django.db.backends.mysql',
      'NAME': 'cidb',
      'USER': 'annotator',
      'PASSWORD': annotator_password}
else:
  # Running in development, using a local development db. Used by unittests.
  # Instead of requiring developers to install mysql, we'll use sqlite, and
  # maintain migration scripts that can be used to create the schema.
  default_database = {
      'ENGINE': 'django.db.backends.sqlite3',
      'NAME': os.path.join(BASE_DIR, 'test_db.sqlite3')}

DATABASES = {'default': default_database}

# Hosts/domain names that are valid for this site; required if DEBUG is False
# See https://docs.djangoproject.com/en/1.5/ref/settings/#allowed-hosts
ALLOWED_HOSTS = ['cros-builder-stats-annotator.googleplex.com']

# Local time zone for this installation. Choices can be found here:
# http://en.wikipedia.org/wiki/List_of_tz_zones_by_name
# although not all choices may be available on all operating systems.
# In a Windows environment this must be set to your system time zone.
TIME_ZONE = 'America/Los_Angeles'

# Language code for this installation. All choices can be found here:
# http://www.i18nguy.com/unicode/language-identifiers.html
LANGUAGE_CODE = 'en-us'

SITE_ID = 1

# If you set this to False, Django will make some optimizations so as not
# to load the internationalization machinery.
USE_I18N = True

# If you set this to False, Django will not format dates, numbers and
# calendars according to the current locale.
USE_L10N = True

# If you set this to False, Django will not use timezone-aware datetimes.
USE_TZ = True

# Absolute filesystem path to the directory that will hold user-uploaded files.
# Example: "/var/www/example.com/media/"
MEDIA_ROOT = ''

# URL that handles the media served from MEDIA_ROOT. Make sure to use a
# trailing slash.
# Examples: "http://example.com/media/", "http://media.example.com/"
MEDIA_URL = ''

# Absolute path to the directory static files should be collected to.
# Don't put anything in this directory yourself; store your static files
# in apps' "static/" subdirectories and in STATICFILES_DIRS.
# Example: "/var/www/example.com/static/"
STATIC_ROOT = os.path.join(PROJECT_DIR, 'static')

# URL prefix for static files.
# Example: "http://example.com/static/", "http://static.example.com/"
STATIC_URL = '/static/'

# Additional locations of static files
STATICFILES_DIRS = (
    # Put strings here, like "/home/html/static" or "C:/www/django/static".
    # Always use forward slashes, even on Windows.
    # Don't forget to use absolute paths, not relative paths.
)

# List of finder classes that know how to find static files in
# various locations.
STATICFILES_FINDERS = (
    'django.contrib.staticfiles.finders.FileSystemFinder',
    'django.contrib.staticfiles.finders.AppDirectoriesFinder',
    # 'django.contrib.staticfiles.finders.DefaultStorageFinder',
)

# Make this unique, and don't share it with anybody.
# TODO(pprabhu): Add secret key to valentine, must be updated before pushing to
# prod.
SECRET_KEY = 'NotSoSecretKeyThatSomeOneDreamtUp'

# List of callables that know how to import templates from various sources.
TEMPLATE_LOADERS = (
    'django.template.loaders.filesystem.Loader',
    'django.template.loaders.app_directories.Loader',
    # 'django.template.loaders.eggs.Loader',
)

MIDDLEWARE_CLASSES = (
    'django.middleware.common.CommonMiddleware',
    'django.contrib.sessions.middleware.SessionMiddleware',
    'django.middleware.csrf.CsrfViewMiddleware',
    # 'django.contrib.auth.middleware.AuthenticationMiddleware',
    'django.contrib.messages.middleware.MessageMiddleware',
    # Uncomment the next line for simple clickjacking protection:
    # 'django.middleware.clickjacking.XFrameOptionsMiddleware',
)

ROOT_URLCONF = 'cq_stats.urls'

# Python dotted path to the WSGI application used by Django's runserver.
WSGI_APPLICATION = 'cq_stats.wsgi.application'

TEMPLATE_DIRS = (
    # Put strings here, like "/home/html/django_templates" or
    # "C:/www/django/templates".
    # Always use forward slashes, even on Windows.
    # Don't forget to use absolute paths, not relative paths.
)

INSTALLED_APPS = (
    # 'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.sites',
    'django.contrib.messages',
    'django.contrib.staticfiles',
    # 'django.contrib.admin',
    # Uncomment the next line to enable admin documentation:
    # 'django.contrib.admindocs',

    # Apps in this project
    'build_annotations'
)

SESSION_SERIALIZER = 'django.contrib.sessions.serializers.JSONSerializer'

# (pprabhu): Cookie based sessions are temporary. They have various drawbacks,
# including a load time penalty if the size grows. OTOH, they are the easiest to
# bringup on AppEngine. Let's use these to get started.
SESSION_ENGINE = 'django.contrib.sessions.backends.signed_cookies'

# A sample logging configuration. The only tangible logging
# performed by this configuration is to send an email to
# the site admins on every HTTP 500 error when DEBUG=False.
# See http://docs.djangoproject.com/en/dev/topics/logging for
# more details on how to customize your logging configuration.
LOGGING = {
    'version': 1,
    'disable_existing_loggers': False,
    'filters': {
        'require_debug_false': {
            '()': 'django.utils.log.RequireDebugFalse'
        }
    },
    'handlers': {
        'mail_admins': {
            'level': 'ERROR',
            'filters': ['require_debug_false'],
            'class': 'django.utils.log.AdminEmailHandler'
        }
    },
    'loggers': {
        'django.request': {
            'handlers': ['mail_admins'],
            'level': 'ERROR',
            'propagate': True,
        },
    }
}
