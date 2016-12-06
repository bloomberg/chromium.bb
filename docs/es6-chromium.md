<!-- This is feature markdown template
## Header

**Usage Example:**

``` js

```

**Documentation:** [link]()

**Discussion Notes / Link to Thread:**

hyphen-hyphen-hyphen (change to actual hyphen)

-->


<style type="text/css">
  .doc {
    font-size: 16px;
  }

  .doc h2[id] {
    line-height: 20px;
    font-size: 16px;
  }

  .doc h2 > code {
    font-size: 16px;
    font-weight: bold;
  }

  .feature-container {
    background-color: #e8eef7;
    border: 1px solid #c3d9ff;
    margin-bottom: 5px;
    border-radius: 5px;
  }

  .feature-container > h2 {
    cursor: pointer;
    background-color: #c3d9ff;
    margin: 0;
    padding: 5px;
    border-radius: 5px;
  }

  .feature-container > *:not(h2){
    display: none;
    padding: 0px 10px;
  }

  .feature-container.open > *:not(h2){
    display: block;
  }
</style>

<script>
document.addEventListener("DOMContentLoaded", function(event) {
  // Move all headers and corresponding contents to an accordion container.
  document.querySelectorAll('h2[id]').forEach(function(header){
    var container = document.createElement('div');
    container.classList.add('feature-container');
    header.parentNode.insertBefore(container, header);

    // Add all the following siblings until it hits an <hr>
    var ele = header;
    while(ele && ele.tagName !== 'HR') {
      var nextEle = ele.nextElementSibling;
      container.append(ele);
      ele = nextEle;
    }

    // Add handler to open accordion on click.
    header.addEventListener('click', () => {
      header.parentNode.classList.toggle('open');
    });
  });

  // Then remove all <hr>s since everything's accordionized.
  document.querySelectorAll('hr').forEach(function(ele){
    ele.parentNode.removeChild(ele);
  });
});
</script>

[TOC]

# ES6 Support In Chromium

This is a list of new/updated features in ES6 specs that is being considered to
be supported for Chromium development.

>**TBD:** Do we want to differenciate allow/ban status between subprojects? If
so, how to denote?

>**TBD:** Cross platform-build support?

You can propose changing the status of a feature by sending an email to
chromium-dev@chromium.org. Include a short blurb on what the feature is and why
you think it should or should not be allowed, along with links to any relevant
previous discussion. If the list arrives at some consensus, send a codereview
to change this file accordingly, linking to your discussion thread.

>Some descriptions and Usage examples are from [kangax](https://kangax.github.
io/compat-table/es6/) and [http://es6-features.org/](http://es6-features.org/)

# Allowed Features

The following features are allowed in Chromium development.

## `Promise`

Built-in representation of a value that might be determined asynchronously,
relieving developers from "callback hell".

**Usage Example:**

``` js
function promiseA() {
  return new Promise((resolve, reject) => setTimeout(resolve, 100));
}

function promiseB() {
  return new Promise((resolve, reject) => setTimeout(resolve, 200));
}

function promiseC() {
  return new Promise((resolve, reject) => setTimeout(resolve, 300));
}

Promise.all([promiseA(), promiseB(), promiseC()]).then(([a, b, c]) => {
  someFunction(a, b, c);
});
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-promise-objects)

**Discussion Notes:** Feature already extensively used prior to creation of
this document.

---

# Banned Features

The following features are banned for Chromium development.

# Features To Be Discussed

The following features are currently disallowed. See the top of this page on
how to propose moving a feature from this list into the allowed or banned
sections.

## `let` (Block-Scoped Variables)

Declare variable that exists within the block scope. `let` can generally be
used to replace `var` but `let` in global scope, unlike `var`, does not
introduce a property on the global object.

**Usage Example:**

``` js
// This will make all buttons output "3".
for(var i = 0; i < 3; i++) {
  buttons[i].onclick = function() {
    console.log(i);
  };
}

// This will make buttons output corresponding "i" values.
for(let i = 0; i < 3; i++) {
  buttons[i].onclick = function() {
    console.log(i);
  };
}

var bar = 1;
var bar = 1; // No error thrown.

let foo = 1;
let foo = 1; // TypeError: Identifier 'foo' has already been declared.
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-let-and-const-declarations)

**Discussion Notes / Link to Thread:**

---

## `const` (Block-Scoped Constants)

Constants (also known as "immutable variables") are variables which cannot be
re-assigned new content. Note that if the value is an object, the object itself
is still mutable.

`const` has traditionally been supported as a "function scoped" declaration
like `var` (except in Internet Explorer), however in VMs supporting ES6 `const`
is now a block scope declaration.

**Usage Example:**

``` js
const gravity = 9.81;
gravity = 0; // TypeError: Assignment to constant variable.

gravity === 9.81; // true
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-let-and-const-declarations)

**Discussion Notes / Link to Thread:**

---

## `=>` (Arrow Functions)

Arrow functions provide a concise syntax to create a function, and fix a number
of difficulties with this (e.g. eliminating the need to write `const self =
this`. Particularly useful for nested functions or callbacks.

Prefer arrow functions over the function keyword, over `f.bind(this)`, and
especially over `goog.bind(f, this)`.

Arrow functions has an implicit return when used without a body block.

**Usage Example:**

``` js
// General usage, eliminating need for .bind(this).
setTimeout(() => {
  this.doSomething();
}, 1000);  // no need for .bind(this) or const self = this.

// Another example...
window.addEventListener('scroll', (event) => {
  this.doSomething(event);
});  // no need for .bind(this) or const self = this.

// Implicit return: returns the value if expression not inside a body block.
() => 1 // returns 1
() => {1} // returns undefined - body block does not implicitly return.
() => {return 1;} // returns 1
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-arrow-function-definitions)

**Discussion Notes / Link to Thread:**

---

## Classes

OOP-style and boilerplate-free class syntax, including inheritance, super(),
static members, and getters and setters.

**Usage Example:**

``` js
class Rectangle extends Shape {
  constructor(id, x, y, width, height) {
    super(id, x, y);
    this.width  = width;
    this.height = height;
  }
  static defaultRectangle() {
    return new Rectangle('default', 0, 0, 100, 100);
  }
  move(x, y) {
    this.x = x;
    this.y = y;
  }
};
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-class-definitions)

**Discussion Notes / Link to Thread:**

---

## Block Scope Functions

**Usage Example:**

``` js
{
  function foo() {
    return 1;
  }
  // foo() === 1
  {
    function foo() {
      return 2;
    }
    // foo() === 2
  }
  // foo() === 1
}
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-functiondeclarationinstantiation)

**Discussion Notes / Link to Thread:**

---

## Default Function Parameters

**Usage Example:**

``` js
function f(x, y = 7, z = 42) {
  return x + y + z;
}
// f(1) === 50;
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-functiondeclarationinstantiation)

**Discussion Notes / Link to Thread:**

---

## Rest Parameters

Aggregation of function arguments into one Array variable.

**Usage Example:**

``` js
function f(x, y, ...a) {
  // for f(1, 2, 3, 4, 5)...
    // x = 1, y = 2
    // a = [3, 4, 5]
}
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-function-definitions)

**Discussion Notes / Link to Thread:**

---

## Spread Operators

Spreading the elements from an iterable collection into individual literals as
function parameters.

**Usage Example:**

``` js
// Spreading an Array
var params = [ 'hello', true, 7 ];
var other = [ 1, 2, ...params ]; // [ 1, 2, 'hello', true, 7 ]
f(1, 2, ...params) === 9;

// Spreading a String
var str = 'foo';
var chars = [ ...str ]; // [ 'f', 'o', 'o' ]
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-argument-lists-runtime-semantics-argumentlistevaluation)

**Discussion Notes / Link to Thread:**

---

## Object Literal Extensions

Convenient new ways for object property definition.

**Usage Example:**

``` js
// Computed property name
var x = 'key';
var obj = {[x]: 1};

// Shorthand property
var obj = {x, y}; //equivalent to {x:x, y:y}

// Method property
var obj = {
  foo() {...},
  bar() {...}
}
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-object-initialiser)

**Discussion Notes / Link to Thread:**

---

## Template Literals

Expression interpolation for Strings, with the ability to access raw template
pieces.

**Usage Example:**

``` js
// Simple example
var greeting = 'hello';
var myName = {first: 'Foo', last: 'Bar'};
var message = `${greeting},
my name is ${myName.first + myName.last}`;
// message == 'hello,\nmy name is FooBar'

// Custom interpolation
function foo (strings, ...values) {
  // for foo`bar${42}baz`...
    // strings[0] === 'bar';
    // strings[1] === 'baz';
    // values[0] === 42;

  return strings[1] + strings[0] + values[0];
}

var newString = foo`bar${42}baz`; // 'bazbar42'
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-template-literals)

**Discussion Notes / Link to Thread:**

---

## Binary & Octal Literals

**Usage Example:**

``` js
0b111110111 === 503;
0o767 === 503;
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-literals-numeric-literals)

**Discussion Notes / Link to Thread:**

---

## `/u` Unicode Regex Literal

**Usage Example:**

``` js
'𠮷'.match(/./u)[0].length === 2;
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-get-regexp.prototype.sticky)

**Discussion Notes / Link to Thread:**

---

## `\u{}` Unicode String

**Usage Example:**

``` js
'\u{1d306}' == '\ud834\udf06'; // true
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-literals-string-literals)

**Discussion Notes / Link to Thread:**

---

## `/y` Regex Sticky Matching

Keep the matching position sticky between matches and this way support
efficient parsing of arbitrary long input strings, even with an arbitrary
number of distinct regular expressions.

**Usage Example:**

``` js
var re = new RegExp('yy', 'y');
re.lastIndex = 3;
var result = re.exec('xxxyyxx')[0];
result === 'yy' && re.lastIndex === 5; // true
```

**Documentation:** [link](http://es6-features.org
/#RegularExpressionStickyMatching)

**Discussion Notes / Link to Thread:**

---

## Destructuring Assignment

Flexible destructuring of collections or parameters.

**Usage Example:**

``` js
// Array
var list = [ 1, 2, 3 ];
var [ a, , b ] = list;
// a = 1, b = 3

// Object
var {width, height, area: a} = rect;
// width = rect.width, height = rect.height, a = rect.area

// Parameters
function f ([ name, val ]) {
  console.log(name, val);
}
function g ({ name: n, val: v }) {
  console.log(n, v);
}
function h ({ name, val }) {
  console.log(name, val);
}

f([ 'bar', 42 ]);
g({ name: 'foo', val:  7 });
h({ name: 'bar', val: 42 });

```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-destructuring-assignment)

**Discussion Notes / Link to Thread:**

---

## Modules

Support for exporting/importing values from/to modules without global
namespace pollution.

**Usage Example:**

``` js
// lib/rect.js
export function getArea() {...};
export { width, height, unimportant };

// app.js
import {getArea, width, height} from 'lib/rect';

```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-modules)

**Discussion Notes / Link to Thread:**

---

## Symbol Type

Unique and immutable data type to be used as an identifier for object
properties.

**Usage Example:**

``` js
const foo = Symbol();
const bar = Symbol();
typeof foo === 'symbol'; // true
typeof bar === 'symbol'; // true
let obj = {};
obj[foo] = 'foo';
obj[bar] = 'bar';
JSON.stringify(obj); // {}
Object.keys(obj); // []
Object.getOwnPropertyNames(obj); // []
Object.getOwnPropertySymbols(obj); // [ foo, bar ]
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-symbol-constructor)

**Discussion Notes / Link to Thread:**

---

## `for ...of` Loops

Convenient operator to iterate over all values of an iterable object.

**Usage Example:**

``` js
// Given an iterable  collection `fibonacci`...
for (let n of fibonacci) {
  console.log(n);
}
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-for-in-and-for-of-statements)

**Discussion Notes / Link to Thread:**

---

## Object Static Methods

**Usage Example:**

``` js
// Object.assign
var o = Object.assign({a:true}, {b:true}, {c:true});
'a' in o && 'b' in o && 'c' in o; // true

// Object.setPrototypeOf
Object.setPrototypeOf({}, Array.prototype) instanceof Array; //true

// Object.is
// Object.getOwnPropertySymbols
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-properties-of-the-object-constructor)

**Discussion Notes / Link to Thread:**

---

## String Static & Prototype methods

**Usage Example:**

``` js
// String.raw
// String.fromCodePoint

// String.prototype.codePointAt
// String.prototype.normalize
// String.prototype.repeat
// String.prototype.startsWith
// String.prototype.endsWith
// String.prototype.includes
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-properties-of-the-string-constructor)

**Discussion Notes / Link to Thread:**

---

## Array Static & Prototype Methods

**Usage Example:**

``` js
// Array.from
// Array.of

// Array.prototype.copyWithin
// Array.prototype.find
// Array.prototype.findIndex
// Array.prototype.fill
// Array.prototype.keys
// Array.prototype.values
// Array.prototype.entries
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-properties-of-the-array-constructor)

**Discussion Notes / Link to Thread:**

---

## Number Properties

**Usage Example:**

``` js
// Number.isFinite
// Number.isInteger
// Number.isSafeInteger
// Number.isNaN
// Number.EPSILON
// Number.MIN_SAFE_INTEGER
// Number.MAX_SAFE_INTEGER
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-isfinite-number)

**Discussion Notes / Link to Thread:**

---

## Iterators

**Usage Example:**

``` js
let fibonacci = {
  [Symbol.iterator]() {
    let pre = 0, cur = 1;
    return {
      next () {
        [ pre, cur ] = [ cur, pre + cur ];
        return { done: false, value: cur };
      }
    };
  }
};
```

**Documentation:** [link]()

**Discussion Notes / Link to Thread:**

---

## Generators

Special iterators with specified pausing points.

**Usage Example:**

``` js
function* range(start, end, step) {
  while (start < end) {
    yield start;
    start += step;
  }
}

for (let i of range(0, 10, 2)) {
  console.log(i); // 0, 2, 4, 6, 8
}

```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-generator-function-definitions)

**Discussion Notes / Link to Thread:**

---

## `Map`

**Usage Example:**

``` js
var key = {};
var map = new Map();

map.set(key, 123);

map.has(key) && map.get(key) === 123; // true
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-map-objects)

**Discussion Notes / Link to Thread:**

---

## `Set`

**Usage Example:**

``` js
var obj = {};
var set = new Set();

set.add(123);
set.add(123);

set.has(123); // true
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-set-objects)

**Discussion Notes / Link to Thread:**

---

## `WeakMap`

WeakMap does not prevent garbage collection if nothing else refers to an object
within the collection.

**Usage Example:**

``` js
var key = {};
var weakmap = new WeakMap();

weakmap.set(key, 123);

weakmap.has(key) && weakmap.get(key) === 123; // true
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-weakmap-objects)

**Discussion Notes / Link to Thread:**

---

## `WeakSet`

WeakSet does not prevent garbage collection if nothing else refers to an object
within the collection.

**Usage Example:**

``` js
var obj1 = {};
var weakset = new WeakSet();

weakset.add(obj1);
weakset.add(obj1);

weakset.has(obj1); // true
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-weakset-objects)

**Discussion Notes / Link to Thread:**

---

## Typed Arrays

A lot of new typed Arrays...

**Usage Example:**

``` js
new Int8Array();
new UInt8Array();
new UInt8ClampedArray()
// ...You get the idea. Click on the Documentation link below to see all.
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-typedarray-objects)

**Discussion Notes / Link to Thread:**

---

## `Proxy`

Hooking into runtime-level object meta-operations.

**Usage Example:**

``` js
let target = {
  foo: 'Welcome, foo'
};
let proxy = new Proxy(target, {
  get (receiver, name) {
    return name in receiver ? receiver[name] : `Hello, ${name}`;
  }
});
proxy.foo === 'Welcome, foo'; // true
proxy.world === 'Hello, world'; // true
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-proxy-object-internal-methods-and-internal-slots)

**Discussion Notes / Link to Thread:**

---

## `Reflection`

Make calls corresponding to the object meta-operations.

**Usage Example:**

``` js
let obj = { a: 1 };
Object.defineProperty(obj, 'b', { value: 2 });
obj[Symbol('c')] = 3;
Reflect.ownKeys(obj); // ['a', 'b', Symbol(c)]
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-reflection)

**Discussion Notes / Link to Thread:**

---

## Math Methods

A lot of new Math methods.

**Usage Example:**

``` js
// See Doc
```

**Documentation:** [link](http://www.ecma-international.org/ecma-262/6.0
/#sec-math)

**Discussion Notes / Link to Thread:**

---
