# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper object to read and modify Shared Preferences from Android apps.

See e.g.:
  http://developer.android.com/reference/android/content/SharedPreferences.html
"""

import collections
import logging
import posixpath

from xml.etree import ElementTree


_XML_DECLARATION = "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"


class BasePref(object):
  """Base class for getting/setting the value of a specific preference type.

  Should not be instantiated directly. The SharedPrefs collection will
  instantiate the appropriate subclasses, which directly manipulate the
  underlying xml document, to parse and serialize values according to their
  type.

  Args:
    elem: An xml ElementTree object holding the preference data.

  Properties:
    tag_name: A string with the tag that must be used for this preference type.
  """
  tag_name = None

  def __init__(self, elem):
    assert elem.tag == type(self).tag_name
    self._elem = elem

  def __str__(self):
    """Get the underlying xml element as a string."""
    return ElementTree.tostring(self._elem)

  def get(self):
    """Get the value of this preference."""
    return self._elem.get('value')

  def set(self, value):
    """Set from a value casted as a string."""
    self._elem.set('value', str(value))

  @property
  def has_value(self):
    """Check whether the element has a value."""
    return self._elem.get('value') is not None


class BooleanPref(BasePref):
  """Class for getting/setting a preference with a boolean value.

  The underlying xml element has the form, e.g.:
      <boolean name="featureEnabled" value="false" />
  """
  tag_name = 'boolean'
  VALUES = {'true': True, 'false': False}

  def get(self):
    """Get the value as a Python bool."""
    return type(self).VALUES[super(BooleanPref, self).get()]

  def set(self, value):
    """Set from a value casted as a bool."""
    super(BooleanPref, self).set('true' if value else 'false')


class FloatPref(BasePref):
  """Class for getting/setting a preference with a float value.

  The underlying xml element has the form, e.g.:
      <float name="someMetric" value="4.7" />
  """
  tag_name = 'float'

  def get(self):
    """Get the value as a Python float."""
    return float(super(FloatPref, self).get())


class IntPref(BasePref):
  """Class for getting/setting a preference with an int value.

  The underlying xml element has the form, e.g.:
      <int name="aCounter" value="1234" />
  """
  tag_name = 'int'

  def get(self):
    """Get the value as a Python int."""
    return int(super(IntPref, self).get())


class LongPref(IntPref):
  """Class for getting/setting a preference with a long value.

  The underlying xml element has the form, e.g.:
      <long name="aLongCounter" value="1234" />

  We use the same implementation from IntPref.
  """
  tag_name = 'long'


class StringPref(BasePref):
  """Class for getting/setting a preference with a string value.

  The underlying xml element has the form, e.g.:
      <string name="someHashValue">249b3e5af13d4db2</string>
  """
  tag_name = 'string'

  def get(self):
    """Get the value as a Python string."""
    return self._elem.text

  def set(self, value):
    """Set from a value casted as a string."""
    self._elem.text = str(value)


class StringSetPref(StringPref):
  """Class for getting/setting a preference with a set of string values.

  The underlying xml element has the form, e.g.:
      <set name="managed_apps">
          <string>com.mine.app1</string>
          <string>com.mine.app2</string>
          <string>com.mine.app3</string>
      </set>
  """
  tag_name = 'set'

  def get(self):
    """Get a list with the string values contained."""
    value = []
    for child in self._elem:
      assert child.tag == 'string'
      value.append(child.text)
    return value

  def set(self, value):
    """Set from a sequence of values, each casted as a string."""
    for child in list(self._elem):
      self._elem.remove(child)
    for item in value:
      ElementTree.SubElement(self._elem, 'string').text = str(item)


_PREF_TYPES = {c.tag_name: c for c in [BooleanPref, FloatPref, IntPref,
                                       LongPref, StringPref, StringSetPref]}


class SharedPrefs(collections.MutableMapping):
  def __init__(self, device, package, filename):
    """Mapping object to read and update "Shared Prefs" of Android apps.

    Such files typically look like, e.g.:

        <?xml version='1.0' encoding='utf-8' standalone='yes' ?>
        <map>
          <int name="databaseVersion" value="107" />
          <boolean name="featureEnabled" value="false" />
          <string name="someHashValue">249b3e5af13d4db2</string>
        </map>

    Example usage:

        prefs = shared_prefs.SharedPrefs(device, 'com.my.app', 'my_prefs.xml')
        prefs.Load()
        prefs['string:someHashValue'] # => '249b3e5af13d4db2'
        prefs['int:databaseVersion'] = 42
        del prefs['boolean:featureEnabled']
        prefs.Commit()

    The object may also be used as a context manager to automatically load and
    commit, respectively, upon entering and leaving the context.

    Args:
      device: A DeviceUtils object.
      package: A string with the package name of the app that owns the shared
        preferences file.
      filename: A string with the name of the preferences file to read/write.
    """
    self._device = device
    self._xml = None
    self._package = package
    self._filename = filename
    self._path = '/data/data/%s/shared_prefs/%s' % (package, filename)
    self._changed = False

  def __repr__(self):
    """Get a useful printable representation of the object."""
    return '<{cls} file {filename} for {package} on {device}>'.format(
      cls=type(self).__name__, filename=self.filename, package=self.package,
      device=str(self._device))

  def __str__(self):
    """Get the underlying xml document as a string."""
    return _XML_DECLARATION + ElementTree.tostring(self.xml)

  @property
  def package(self):
    """Get the package name of the app that owns the shared preferences."""
    return self._package

  @property
  def filename(self):
    """Get the filename of the shared preferences file."""
    return self._filename

  @property
  def path(self):
    """Get the full path to the shared preferences file on the device."""
    return self._path

  @property
  def changed(self):
    """True if properties have changed and a commit would be needed."""
    return self._changed

  @property
  def xml(self):
    """Get the underlying xml document as an ElementTree object."""
    if self._xml is None:
      self._xml = ElementTree.Element('map')
    return self._xml

  def Load(self):
    """Load the shared preferences file from the device.

    A empty xml document, which may be modified and saved on |commit|, is
    created if the file does not already exist.
    """
    if self._device.FileExists(self.path):
      self._xml = ElementTree.fromstring(
          self._device.ReadFile(self.path, as_root=True))
      assert self._xml.tag == 'map'
    else:
      self._xml = None
    self._changed = False

  def Clear(self):
    """Clear all of the preferences contained in this object."""
    if self._xml is not None and len(self): # only clear if not already empty
      self._xml = None
      self._changed = True

  def Commit(self):
    """Save the current set of preferences to the device.

    Only actually saves if some preferences have been modified.
    """
    if not self.changed:
      return
    self._device.RunShellCommand(
        ['mkdir', '-p', posixpath.dirname(self.path)],
        as_root=True, check_return=True)
    self._device.WriteFile(self.path, str(self), as_root=True)
    self._device.KillAll(self.package, as_root=True, quiet=True)
    self._changed = False

  def __len__(self):
    """Get the number of preferences contained in this object."""
    return len(self.xml)

  def __iter__(self):
    """Iterate over the preference keys, each of the form: 'type:name'."""
    for child in self.xml:
      yield ':'.join([child.tag, child.get('name')])

  def __getitem__(self, key):
    """Get the value of a preference.

    Raises:
      TypeError when the key is not of the form 'type:name' for a valid type.
      KeyError when the key is not found in the collection.
    """
    return self._GetPref(key).get()

  def __setitem__(self, key, value):
    """Set the value of a preference.

    Raises:
      TypeError when the key is not of the form 'type:name' for a valid type.
    """
    pref = self._GetPref(key, create=True)
    if not pref.has_value or pref.get() != value:
      pref.set(value)
      self._changed = True
      logging.info('Setting property: %s', pref)

  def __delitem__(self, key):
    """Delete a preference.

    Raises:
      TypeError when the key is not of the form 'type:name' for a valid type.
      KeyError when the key is not found in the collection.
    """
    self.xml.remove(self._GetPref(key)._elem)

  def AsDict(self):
    """Return the properties and their values as a dictionary."""
    return dict(self.iteritems())

  def __enter__(self):
    """Load preferences file from the device when entering a context."""
    self.Load()
    return self

  def __exit__(self, exc_type, _exc_value, _traceback):
    """Save preferences file to the device when leaving a context."""
    if not exc_type:
      self.Commit()

  def _GetPref(self, key, create=False):
    """Find a preference by key, possibly creating one if it doesn't exist."""
    if ':' not in key:
      raise TypeError('Invalid preference key %r' % key)
    tag, name = key.split(':', 1)
    try:
      mk_pref = _PREF_TYPES[tag]
    except KeyError:
      raise TypeError('Invalid preference key %r' % key)
    for child in self.xml:
      if child.get('name') == name:
        return mk_pref(child)
    if create:
      child = ElementTree.SubElement(self.xml, tag, {'name': name})
      self._changed = True
      return mk_pref(child)
    else:
      raise KeyError(key)
