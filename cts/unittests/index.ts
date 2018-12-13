import test from 'mytape';

test('fail', t => {
  t.true(false);
});

test('sync', t => {
  t.true(true);
});

test('async', async t => {
  await new Promise(res => {
    setTimeout(res, 100);
  })
  t.true(true);
});
