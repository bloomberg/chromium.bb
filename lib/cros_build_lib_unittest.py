#!/usr/bin/python

import errno
import os 
import unittest 
import shutil
import tempfile 

import cros_build_lib

class TestListFiles(unittest.TestCase):

  def setUp(self):
    self.root_dir = tempfile.mkdtemp(prefix='listfiles_unittest')

  def tearDown(self):
    shutil.rmtree(self.root_dir)

  def _createNestedDir(self, dir_structure):
    for entry in dir_structure:
      full_path = os.path.join(os.path.join(self.root_dir, entry))
      # ensure dirs are created
      try:
        os.makedirs(os.path.dirname(full_path))
        if full_path.endswith('/'):
          # we only want to create directories
          return
      except OSError, err:
        if err.errno == errno.EEXIST:
          # we don't care if the dir already exists
          pass
        else:
          raise
      # create dummy files
      tmp = open(full_path, 'w')
      tmp.close()

  def testTraverse(self):
    """
    Test that we are traversing the directory properly
    """
    dir_structure = ['one/two/test.txt', 'one/blah.py',
                     'three/extra.conf']
    self._createNestedDir(dir_structure)

    files = cros_build_lib.ListFiles(self.root_dir)
    for file in files:
      file = file.replace(self.root_dir, '').lstrip('/')
      if file not in dir_structure:
        self.fail('%s was not found in %s' % (file, dir_structure))

  def testEmptyFilePath(self):
    """
    Test that we return nothing when directories are empty
    """
    dir_structure = ['one/', 'two/', 'one/a/']
    self._createNestedDir(dir_structure)
    files = cros_build_lib.ListFiles(self.root_dir)
    self.assertEqual(files, [])

  def testNoSuchDir(self):
    try:
      cros_build_lib.ListFiles('/me/no/existe')
    except OSError, err:
      self.assertEqual(err.errno, errno.ENOENT)
    

if __name__ == '__main__':
  unittest.main()
