# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setups a local Rietveld instance to test against a live server for
integration tests.

It makes sure Google AppEngine SDK is found, download Rietveld and Django code
if necessary and starts the server on a free inbound TCP port.
"""

import os
import socket
import subprocess
import time


class Failure(Exception):
    pass


def test_port(port):
    s = socket.socket()
    try:
        return s.connect_ex(('127.0.0.1', port)) == 0
    finally:
        s.close()


def find_free_port():
    # Test to find an available port starting at 8080.
    port = 8080
    max_val = (2<<16)
    while test_port(port) and port < max_val:
        port += 1
    if port == max_val:
        raise Failure('Having issues finding an available port')
    return port


class LocalRietveld(object):
    """Downloads everything needed to run a local instance of Rietveld."""

    def __init__(self, base_dir=None):
        # Paths
        self.base_dir = base_dir
        if not self.base_dir:
            self.base_dir = os.path.dirname(os.path.abspath(__file__))
            self.base_dir = os.path.realpath(os.path.join(self.base_dir, '..'))
        self.sdk_path = os.path.abspath(
            os.path.join(self.base_dir, '..', '..', 'google_appengine'))
        self.dev_app = os.path.join(self.sdk_path, 'dev_appserver.py')
        self.rietveld = os.path.join(self.base_dir, 'test', 'rietveld')
        self.django_path = os.path.join(self.rietveld, 'django')
        self.test_server = None
        self.port = None
        # Generate a friendly environment.
        self.env = os.environ.copy()
        self.env['LANGUAGE'] = 'en'
        if self.env.get('PYTHONPATH'):
            self.env['PYTHONPATH'] = (
                self.env['PYTHONPATH'].rstrip(os.pathsep) + os.pathsep +
                self.django_path)
        else:
            self.env['PYTHONPATH'] = self.django_path

    def install_prerequisites(self):
        # First, verify the Google AppEngine SDK is available.
        if not os.path.isfile(self.dev_app):
            raise Failure('Install google_appengine sdk in %s' % self.sdk_path)

        def call(*args, **kwargs):
            kwargs['env'] = self.env
            x = subprocess.Popen(*args, **kwargs)
            x.communicate()
            return x.returncode == 0

        # Second, checkout rietveld and django if not available.
        if not os.path.isdir(self.rietveld):
            print('Checking out rietveld...')
            if not call(
                ['svn', 'co', '-q',
                    'http://rietveld.googlecode.com/svn/trunk@563',
                    self.rietveld]):
                raise Failure('Failed to checkout rietveld')
        if not os.path.isdir(self.django_path):
            print('Checking out django...')
            if not call(
                ['svn', 'co', '-q',
                    'http://code.djangoproject.com/'
                        'svn/django/branches/releases/1.0.X/django@13637',
                    self.django_path]):
                raise Failure('Failed to checkout django')

    def start_server(self, verbose=False):
        self.install_prerequisites()
        self.port = find_free_port()
        if verbose:
            self.out = None
            self.err = None
        else:
            self.out = open(os.devnull, 'w')
            self.err = open(os.devnull, 'w')
        output = []
        self.test_server = subprocess.Popen(
            [self.dev_app, self.rietveld, '--port=%d' % self.port,
            '--datastore_path=' + os.path.join(self.rietveld, 'tmp.db'), '-c'],
            stdout=self.out, stderr=self.err, env=self.env)
        # Loop until port 127.0.0.1:port opens or the process dies.
        while not test_port(self.port):
            self.test_server.poll()
            if self.test_server.returncode is not None:
                raise Failure(
                    'Test rietveld instance failed early on port %s' %
                    self.port)
            time.sleep(0.001)

    def stop_server(self):
        if self.test_server:
            self.test_server.kill()
            self.test_server = None
            self.port = None
        if self.out:
            self.out.close()
            self.out = None
        if self.err:
            self.err.close()
            self.err = None


def main():
    print LocalRietveld().start_server()


if __name__ == '__main__':
    main()

# vim: ts=4:sw=4:tw=80:et:
