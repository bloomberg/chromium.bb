#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import os
import posixpath
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(TOOLS_DIR, 'lib', 'tests', 'data')
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(TOOLS_DIR)))
MOCK_DIR = os.path.join(CHROME_SRC, 'third_party', 'pymock')

# For the mock library
sys.path.append(MOCK_DIR)
sys.path.append(TOOLS_DIR)

import build_paths
import create_nmf
import getos
import mock

TOOLCHAIN_OUT = os.path.join(build_paths.OUT_DIR, 'sdk_tests', 'toolchain')
NACL_X86_GLIBC_TOOLCHAIN = os.path.join(TOOLCHAIN_OUT,
                                        '%s_x86' % getos.GetPlatform(),
                                        'nacl_x86_glibc')

PosixRelPath = create_nmf.PosixRelPath


def StripSo(name):
  """Strip trailing hexidecimal characters from the name of a shared object.

  It strips everything after the last '.' in the name, and checks that the new
  name ends with .so.

  e.g.

  libc.so.ad6acbfa => libc.so
  foo.bar.baz => foo.bar.baz
  """
  stripped_name = '.'.join(name.split('.')[:-1])
  if stripped_name.endswith('.so'):
    return stripped_name
  return name


class TestPosixRelPath(unittest.TestCase):
  def testBasic(self):
    # Note that PosixRelPath only converts from native path format to posix
    # path format, that's why we have to use os.path.join here.
    path = os.path.join(os.path.sep, 'foo', 'bar', 'baz.blah')
    start = os.path.sep + 'foo'
    self.assertEqual(PosixRelPath(path, start), 'bar/baz.blah')


class TestDefaultLibpath(unittest.TestCase):
  def testWithoutNaClSDKRoot(self):
    """GetDefaultLibPath wihtout NACL_SDK_ROOT set

    In the absence of NACL_SDK_ROOT GetDefaultLibPath should
    return the empty list."""
    with mock.patch.dict('os.environ', clear=True):
      paths = create_nmf.GetDefaultLibPath('Debug')
    self.assertEqual(paths, [])

  def testHonorNaClSDKRoot(self):
    with mock.patch.dict('os.environ', {'NACL_SDK_ROOT': '/dummy/path'}):
      paths = create_nmf.GetDefaultLibPath('Debug')
    for path in paths:
      self.assertTrue(path.startswith('/dummy/path'))

  def testIncludesNaClPorts(self):
    with mock.patch.dict('os.environ', {'NACL_SDK_ROOT': '/dummy/path'}):
      paths = create_nmf.GetDefaultLibPath('Debug')
    self.assertTrue(any(os.path.join('ports', 'lib') in p for p in paths),
                    "naclports libpath missing: %s" % str(paths))


class TestNmfUtils(unittest.TestCase):
  """Tests for the main NmfUtils class in create_nmf."""

  def setUp(self):
    self.tempdir = None
    self.toolchain = NACL_X86_GLIBC_TOOLCHAIN
    self.objdump = os.path.join(self.toolchain, 'bin', 'i686-nacl-objdump')
    if os.name == 'nt':
      self.objdump += '.exe'
    self._Mktemp()

  def _CreateTestNexe(self, name, arch):
    """Create an empty test .nexe file for use in create_nmf tests.

    This is used rather than checking in test binaries since the
    checked in binaries depend on .so files that only exist in the
    certain SDK that build them.
    """
    compiler = os.path.join(self.toolchain, 'bin', '%s-nacl-g++' % arch)
    if os.name == 'nt':
      compiler += '.exe'
      os.environ['CYGWIN'] = 'nodosfilewarning'
    program = 'int main() { return 0; }'
    name = os.path.join(self.tempdir, name)
    dst_dir = os.path.dirname(name)
    if not os.path.exists(dst_dir):
      os.makedirs(dst_dir)
    cmd = [compiler, '-pthread', '-x' , 'c', '-o', name, '-']
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    p.communicate(input=program)
    self.assertEqual(p.returncode, 0)
    return name

  def tearDown(self):
    if self.tempdir:
      shutil.rmtree(self.tempdir)

  def _Mktemp(self):
    self.tempdir = tempfile.mkdtemp()

  def _CreateNmfUtils(self, nexes, **kwargs):
    if not kwargs.get('lib_path'):
      # Use lib instead of lib64 (lib64 is a symlink to lib).
      kwargs['lib_path'] = [
          os.path.join(self.toolchain, 'x86_64-nacl', 'lib'),
          os.path.join(self.toolchain, 'x86_64-nacl', 'lib32')]
    return create_nmf.NmfUtils(nexes,
                               objdump=self.objdump,
                               **kwargs)

  def _CreateStatic(self, arch_path=None, **kwargs):
    """Copy all static .nexe files from the DATA_DIR to a temporary directory.

    Args:
      arch_path: A dictionary mapping architecture to the directory to generate
          the .nexe for the architecture in.
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .nexe paths
    """
    arch_path = arch_path or {}
    nexes = []
    for arch in ('x86_64', 'x86_32', 'arm'):
      nexe_name = 'test_static_%s.nexe' % arch
      src_nexe = os.path.join(DATA_DIR, nexe_name)
      dst_nexe = os.path.join(self.tempdir, arch_path.get(arch, ''), nexe_name)
      dst_dir = os.path.dirname(dst_nexe)
      if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)
      shutil.copy(src_nexe, dst_nexe)
      nexes.append(dst_nexe)

    nexes.sort()
    nmf_utils = self._CreateNmfUtils(nexes, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())
    return nmf, nexes

  def _CreateDynamicAndStageDeps(self, arch_path=None, **kwargs):
    """Create dynamic .nexe files and put them in a temporary directory, with
    their dependencies staged in the same directory.

    Args:
      arch_path: A dictionary mapping architecture to the directory to generate
          the .nexe for the architecture in.
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .nexe paths
    """
    arch_path = arch_path or {}
    nexes = []
    for arch in ('x86_64', 'x86_32'):
      nexe_name = 'test_dynamic_%s.nexe' % arch
      rel_nexe = os.path.join(arch_path.get(arch, ''), nexe_name)
      arch_alt = 'i686' if arch == 'x86_32' else arch
      nexe = self._CreateTestNexe(rel_nexe, arch_alt)
      nexes.append(nexe)

    nexes.sort()
    nmf_utils = self._CreateNmfUtils(nexes, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())
    nmf_utils.StageDependencies(self.tempdir)

    return nmf, nexes

  def _CreatePexe(self, **kwargs):
    """Copy test.pexe from the DATA_DIR to a temporary directory.

    Args:
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .pexe paths
    """
    pexe_name = 'test.pexe'
    src_pexe = os.path.join(DATA_DIR, pexe_name)
    dst_pexe = os.path.join(self.tempdir, pexe_name)
    shutil.copy(src_pexe, dst_pexe)

    pexes = [dst_pexe]
    nmf_utils = self._CreateNmfUtils(pexes, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())

    return nmf, pexes

  def _CreateBitCode(self, **kwargs):
    """Copy test.bc from the DATA_DIR to a temporary directory.

    Args:
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .bc paths
    """
    bc_name = 'test.bc'
    src_bc = os.path.join(DATA_DIR, bc_name)
    dst_bc = os.path.join(self.tempdir, bc_name)
    shutil.copy(src_bc, dst_bc)

    bcs = [dst_bc]
    nmf_utils = self._CreateNmfUtils(bcs, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())

    return nmf, bcs

  def assertManifestEquals(self, manifest, expected):
    """Compare two manifest dictionaries.

    The input manifest is regenerated with all string keys and values being
    processed through StripSo, to remove the random hexidecimal characters at
    the end of shared object names.

    Args:
      manifest: The generated manifest.
      expected: The expected manifest.
    """
    def StripSoCopyDict(d):
      new_d = {}
      for k, v in d.iteritems():
        new_k = StripSo(k)
        if isinstance(v, (str, unicode)):
          new_v = StripSo(v)
        elif type(v) is list:
          new_v = v[:]
        elif type(v) is dict:
          new_v = StripSoCopyDict(v)
        else:
          # Assume that anything else can be copied directly.
          new_v = v

        new_d[new_k] = new_v
      return new_d

    self.assertEqual(StripSoCopyDict(manifest), expected)

  def assertStagingEquals(self, expected):
    """Compare the contents of the temporary directory, to an expected
    directory layout.

    Args:
      expected: The expected directory layout.
    """
    all_files = []
    for root, _, files in os.walk(self.tempdir):
      rel_root_posix = PosixRelPath(root, self.tempdir)
      for f in files:
        path = posixpath.join(rel_root_posix, StripSo(f))
        if path.startswith('./'):
          path = path[2:]
        all_files.append(path)
    self.assertEqual(set(expected), set(all_files))


  def testStatic(self):
    nmf, _ = self._CreateStatic()
    expected_manifest = {
      'files': {},
      'program': {
        'x86-64': {'url': 'test_static_x86_64.nexe'},
        'x86-32': {'url': 'test_static_x86_32.nexe'},
        'arm': {'url': 'test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testStaticWithPath(self):
    arch_dir = {'x86_32': 'x86_32', 'x86_64': 'x86_64', 'arm': 'arm'}
    nmf, _ = self._CreateStatic(arch_dir, nmf_root=self.tempdir)
    expected_manifest = {
      'files': {},
      'program': {
        'x86-32': {'url': 'x86_32/test_static_x86_32.nexe'},
        'x86-64': {'url': 'x86_64/test_static_x86_64.nexe'},
        'arm': {'url': 'arm/test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testStaticWithPathNoNmfRoot(self):
    # This case is not particularly useful, but it is similar to how create_nmf
    # used to work. If there is no nmf_root given, all paths are relative to
    # the first nexe passed on the commandline. I believe the assumption
    # previously was that all .nexes would be in the same directory.
    arch_dir = {'x86_32': 'x86_32', 'x86_64': 'x86_64', 'arm': 'arm'}
    nmf, _ = self._CreateStatic(arch_dir)
    expected_manifest = {
      'files': {},
      'program': {
        'x86-32': {'url': '../x86_32/test_static_x86_32.nexe'},
        'x86-64': {'url': '../x86_64/test_static_x86_64.nexe'},
        'arm': {'url': 'test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testStaticWithNexePrefix(self):
    nmf, _ = self._CreateStatic(nexe_prefix='foo')
    expected_manifest = {
      'files': {},
      'program': {
        'x86-64': {'url': 'foo/test_static_x86_64.nexe'},
        'x86-32': {'url': 'foo/test_static_x86_32.nexe'},
        'arm': {'url': 'foo/test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testDynamic(self):
    nmf, nexes = self._CreateDynamicAndStageDeps()
    expected_manifest = {
      'files': {
        'main.nexe': {
          'x86-32': {'url': 'test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'test_dynamic_x86_64.nexe'},
        },
        'libc.so': {
          'x86-32': {'url': 'lib32/libc.so'},
          'x86-64': {'url': 'lib64/libc.so'},
        },
        'libgcc_s.so': {
          'x86-32': {'url': 'lib32/libgcc_s.so'},
          'x86-64': {'url': 'lib64/libgcc_s.so'},
        },
        'libpthread.so': {
          'x86-32': {'url': 'lib32/libpthread.so'},
          'x86-64': {'url': 'lib64/libpthread.so'},
        },
      },
      'program': {
        'x86-32': {'url': 'lib32/runnable-ld.so'},
        'x86-64': {'url': 'lib64/runnable-ld.so'},
      }
    }

    expected_staging = [os.path.basename(f) for f in nexes]
    expected_staging.extend([
      'lib32/libc.so',
      'lib32/libgcc_s.so',
      'lib32/libpthread.so',
      'lib32/runnable-ld.so',
      'lib64/libc.so',
      'lib64/libgcc_s.so',
      'lib64/libpthread.so',
      'lib64/runnable-ld.so'])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testDynamicWithPath(self):
    arch_dir = {'x86_64': 'x86_64', 'x86_32': 'x86_32'}
    nmf, nexes = self._CreateDynamicAndStageDeps(arch_dir,
                                                 nmf_root=self.tempdir)
    expected_manifest = {
      'files': {
        'main.nexe': {
          'x86-32': {'url': 'x86_32/test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'x86_64/test_dynamic_x86_64.nexe'},
        },
        'libc.so': {
          'x86-32': {'url': 'x86_32/lib32/libc.so'},
          'x86-64': {'url': 'x86_64/lib64/libc.so'},
        },
        'libgcc_s.so': {
          'x86-32': {'url': 'x86_32/lib32/libgcc_s.so'},
          'x86-64': {'url': 'x86_64/lib64/libgcc_s.so'},
        },
        'libpthread.so': {
          'x86-32': {'url': 'x86_32/lib32/libpthread.so'},
          'x86-64': {'url': 'x86_64/lib64/libpthread.so'},
        },
      },
      'program': {
        'x86-32': {'url': 'x86_32/lib32/runnable-ld.so'},
        'x86-64': {'url': 'x86_64/lib64/runnable-ld.so'},
      }
    }

    expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
    expected_staging.extend([
      'x86_32/lib32/libc.so',
      'x86_32/lib32/libgcc_s.so',
      'x86_32/lib32/libpthread.so',
      'x86_32/lib32/runnable-ld.so',
      'x86_64/lib64/libc.so',
      'x86_64/lib64/libgcc_s.so',
      'x86_64/lib64/libpthread.so',
      'x86_64/lib64/runnable-ld.so'])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testDynamicWithRelPath(self):
    """Test that when the nmf root is a relative path that things work."""
    arch_dir = {'x86_64': 'x86_64', 'x86_32': 'x86_32'}
    old_path = os.getcwd()
    try:
      os.chdir(self.tempdir)
      nmf, nexes = self._CreateDynamicAndStageDeps(arch_dir, nmf_root='')
      expected_manifest = {
        'files': {
          'main.nexe': {
            'x86-32': {'url': 'x86_32/test_dynamic_x86_32.nexe'},
            'x86-64': {'url': 'x86_64/test_dynamic_x86_64.nexe'},
          },
          'libc.so': {
            'x86-32': {'url': 'x86_32/lib32/libc.so'},
            'x86-64': {'url': 'x86_64/lib64/libc.so'},
          },
          'libgcc_s.so': {
            'x86-32': {'url': 'x86_32/lib32/libgcc_s.so'},
            'x86-64': {'url': 'x86_64/lib64/libgcc_s.so'},
          },
          'libpthread.so': {
            'x86-32': {'url': 'x86_32/lib32/libpthread.so'},
            'x86-64': {'url': 'x86_64/lib64/libpthread.so'},
          },
        },
        'program': {
          'x86-32': {'url': 'x86_32/lib32/runnable-ld.so'},
          'x86-64': {'url': 'x86_64/lib64/runnable-ld.so'},
        }
      }

      expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
      expected_staging.extend([
        'x86_32/lib32/libc.so',
        'x86_32/lib32/libgcc_s.so',
        'x86_32/lib32/libpthread.so',
        'x86_32/lib32/runnable-ld.so',
        'x86_64/lib64/libc.so',
        'x86_64/lib64/libgcc_s.so',
        'x86_64/lib64/libpthread.so',
        'x86_64/lib64/runnable-ld.so'])

      self.assertManifestEquals(nmf, expected_manifest)
      self.assertStagingEquals(expected_staging)
    finally:
      os.chdir(old_path)

  def testDynamicWithPathNoArchPrefix(self):
    arch_dir = {'x86_64': 'x86_64', 'x86_32': 'x86_32'}
    nmf, nexes = self._CreateDynamicAndStageDeps(arch_dir,
                                                 nmf_root=self.tempdir,
                                                 no_arch_prefix=True)
    expected_manifest = {
      'files': {
        'main.nexe': {
          'x86-32': {'url': 'x86_32/test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'x86_64/test_dynamic_x86_64.nexe'},
        },
        'libc.so': {
          'x86-32': {'url': 'x86_32/libc.so'},
          'x86-64': {'url': 'x86_64/libc.so'},
        },
        'libgcc_s.so': {
          'x86-32': {'url': 'x86_32/libgcc_s.so'},
          'x86-64': {'url': 'x86_64/libgcc_s.so'},
        },
        'libpthread.so': {
          'x86-32': {'url': 'x86_32/libpthread.so'},
          'x86-64': {'url': 'x86_64/libpthread.so'},
        },
      },
      'program': {
        'x86-32': {'url': 'x86_32/runnable-ld.so'},
        'x86-64': {'url': 'x86_64/runnable-ld.so'},
      }
    }

    expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
    expected_staging.extend([
      'x86_32/libc.so',
      'x86_32/libgcc_s.so',
      'x86_32/libpthread.so',
      'x86_32/runnable-ld.so',
      'x86_64/libc.so',
      'x86_64/libgcc_s.so',
      'x86_64/libpthread.so',
      'x86_64/runnable-ld.so'])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testDynamicWithLibPrefix(self):
    nmf, nexes = self._CreateDynamicAndStageDeps(lib_prefix='foo')
    expected_manifest = {
      'files': {
        'main.nexe': {
          'x86-32': {'url': 'test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'test_dynamic_x86_64.nexe'},
        },
        'libc.so': {
          'x86-32': {'url': 'foo/lib32/libc.so'},
          'x86-64': {'url': 'foo/lib64/libc.so'},
        },
        'libgcc_s.so': {
          'x86-32': {'url': 'foo/lib32/libgcc_s.so'},
          'x86-64': {'url': 'foo/lib64/libgcc_s.so'},
        },
        'libpthread.so': {
          'x86-32': {'url': 'foo/lib32/libpthread.so'},
          'x86-64': {'url': 'foo/lib64/libpthread.so'},
        },
      },
      'program': {
        'x86-32': {'url': 'foo/lib32/runnable-ld.so'},
        'x86-64': {'url': 'foo/lib64/runnable-ld.so'},
      }
    }

    expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
    expected_staging.extend([
      'foo/lib32/libc.so',
      'foo/lib32/libgcc_s.so',
      'foo/lib32/libpthread.so',
      'foo/lib32/runnable-ld.so',
      'foo/lib64/libc.so',
      'foo/lib64/libgcc_s.so',
      'foo/lib64/libpthread.so',
      'foo/lib64/runnable-ld.so'])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testPexe(self):
    nmf, _ = self._CreatePexe()
    expected_manifest = {
      'program': {
        'portable': {
          'pnacl-translate': {
            'url': 'test.pexe'
          }
        }
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testPexeOptLevel(self):
    nmf, _ = self._CreatePexe(pnacl_optlevel=2)
    expected_manifest = {
      'program': {
        'portable': {
          'pnacl-translate': {
            'url': 'test.pexe',
            'optlevel': 2,
          }
        }
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testBitCode(self):
    nmf, _ = self._CreateBitCode(pnacl_debug_optlevel=0)
    expected_manifest = {
      'program': {
        'portable': {
          'pnacl-debug': {
            'url': 'test.bc',
            'optlevel': 0,
          }
        }
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)


if __name__ == '__main__':
  unittest.main()
